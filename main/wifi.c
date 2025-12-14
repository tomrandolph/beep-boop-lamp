#include "esp_event.h"         //for wifi event
#include "esp_log.h"           //for showing logs
#include "esp_system.h"        //esp_init funtions esp_err_t
#include "esp_wifi.h"          //esp_wifi_init functions and wifi operations
#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "freertos/task.h"
#include "lwip/err.h"  //light weight ip packets error handling
#include "lwip/sys.h"  //system applications for light weight ip apps
#include "nvs_flash.h" //non volatile storage
#include <stdio.h>     //for basic printf commands
#include <string.h>    //for handling strings

#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS
#define WIFI_RETRY_NUM 10
#define MODULE_TAG "WIFI"
static int retry_num = 0;

typedef enum {
  WIFI_DISCONNECTED = 0,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_GOT_IP,
} wifi_state_t;
static void (*on_wifi_connected_handler)(void) = NULL;
static wifi_state_t wifi_connection_state = WIFI_DISCONNECTED;

static const char *get_disconnect_reason_string(wifi_err_reason_t reason) {
  switch (reason) {
  case WIFI_REASON_UNSPECIFIED:
    return "UNSPECIFIED";
  case WIFI_REASON_AUTH_EXPIRE:
    return "AUTH_EXPIRE";
  case WIFI_REASON_AUTH_LEAVE:
    return "AUTH_LEAVE";
  case WIFI_REASON_ASSOC_EXPIRE:
    return "ASSOC_EXPIRE";
  case WIFI_REASON_ASSOC_TOOMANY:
    return "ASSOC_TOOMANY";
  case WIFI_REASON_NOT_AUTHED:
    return "NOT_AUTHED";
  case WIFI_REASON_NOT_ASSOCED:
    return "NOT_ASSOCED";
  case WIFI_REASON_ASSOC_LEAVE:
    return "ASSOC_LEAVE";
  case WIFI_REASON_ASSOC_NOT_AUTHED:
    return "ASSOC_NOT_AUTHED";
  case WIFI_REASON_DISASSOC_PWRCAP_BAD:
    return "DISASSOC_PWRCAP_BAD";
  case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
    return "DISASSOC_SUPCHAN_BAD";
  case WIFI_REASON_IE_INVALID:
    return "IE_INVALID";
  case WIFI_REASON_MIC_FAILURE:
    return "MIC_FAILURE";
  case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    return "4WAY_HANDSHAKE_TIMEOUT (wrong password?)";
  case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    return "GROUP_KEY_UPDATE_TIMEOUT";
  case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    return "IE_IN_4WAY_DIFFERS";
  case WIFI_REASON_GROUP_CIPHER_INVALID:
    return "GROUP_CIPHER_INVALID";
  case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
    return "PAIRWISE_CIPHER_INVALID";
  case WIFI_REASON_AKMP_INVALID:
    return "AKMP_INVALID";
  case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
    return "UNSUPP_RSN_IE_VERSION";
  case WIFI_REASON_INVALID_RSN_IE_CAP:
    return "INVALID_RSN_IE_CAP";
  case WIFI_REASON_802_1X_AUTH_FAILED:
    return "802_1X_AUTH_FAILED";
  case WIFI_REASON_CIPHER_SUITE_REJECTED:
    return "CIPHER_SUITE_REJECTED";
  case WIFI_REASON_BEACON_TIMEOUT:
    return "BEACON_TIMEOUT";
  case WIFI_REASON_NO_AP_FOUND:
    return "NO_AP_FOUND (AP not in range or SSID wrong?)";
  case WIFI_REASON_AUTH_FAIL:
    return "AUTH_FAIL";
  case WIFI_REASON_ASSOC_FAIL:
    return "ASSOC_FAIL";
  case WIFI_REASON_HANDSHAKE_TIMEOUT:
    return "HANDSHAKE_TIMEOUT";
  default:
    return "UNKNOWN";
  }
}
static void wifi_event_handler(void *event_handler_arg,
                               esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
  if (event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(MODULE_TAG, "WIFI CONNECTING....\n");
    wifi_connection_state = WIFI_CONNECTING;
  } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
    wifi_connection_state = WIFI_CONNECTED;
    retry_num = 0;

    ESP_LOGI(MODULE_TAG, "WiFi CONNECTED\n");
  } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_connection_state = WIFI_DISCONNECTED;
    wifi_event_sta_disconnected_t *disconnected =
        (wifi_event_sta_disconnected_t *)event_data;
    ESP_LOGW(MODULE_TAG, "WiFi lost connection. Reason: %d (0x%x) - %s",
             disconnected->reason, disconnected->reason,
             get_disconnect_reason_string(disconnected->reason));
    if (retry_num < WIFI_RETRY_NUM) {
      vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second before retry
      esp_wifi_connect();
      retry_num++;
      ESP_LOGI(MODULE_TAG, "Retrying to Connect... (attempt %d/%d)\n",
               retry_num, WIFI_RETRY_NUM);
    } else {
      ESP_LOGE(MODULE_TAG, "WiFi connection failed after %d attempts",
               WIFI_RETRY_NUM);
    }
  } else if (event_id == IP_EVENT_STA_GOT_IP) {
    wifi_connection_state = WIFI_GOT_IP;
    ESP_LOGI(MODULE_TAG, "Wifi got IP...\n\n");
    if (on_wifi_connected_handler) {
      ESP_LOGI(MODULE_TAG, "Calling on_wifi_connected_callback");
      on_wifi_connected_handler();
    }
  }
}
void wifi_connection(void (*on_wifi_connected_callback)(void)) {
  on_wifi_connected_handler = on_wifi_connected_callback;
  retry_num = 0; // Reset retry counter

  // Initialize NVS - required for WiFi to store configuration
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    ESP_LOGW(MODULE_TAG, "NVS partition needs to be erased, erasing...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  esp_netif_init();                // network interdace initialization
  esp_event_loop_create_default(); // responsible for handling and dispatching
                                   // events
  esp_netif_create_default_wifi_sta(); // sets up necessary data structs for
                                       // wifi station interface
  wifi_init_config_t wifi_initiation =
      WIFI_INIT_CONFIG_DEFAULT(); // sets up wifi wifi_init_config struct with
                                  // default values
  esp_wifi_init(
      &wifi_initiation); // wifi initialised with dafault wifi_initiation
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler,
                             NULL); // creating event handler register for wifi
  esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler,
      NULL); // creating event handler register for ip event
  wifi_config_t wifi_config = {0}; // Zero-initialize the structure
  strncpy((char *)wifi_config.sta.ssid, WIFI_SSID,
          sizeof(wifi_config.sta.ssid) - 1);
  wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
  strncpy((char *)wifi_config.sta.password, WIFI_PASS,
          sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  // Disable PMF initially - some routers don't support it properly
  wifi_config.sta.pmf_cfg.capable = false;
  wifi_config.sta.pmf_cfg.required = false;
  // Configure scan to help find the AP
  wifi_config.sta.scan_method = WIFI_FAST_SCAN;
  wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  wifi_config.sta.threshold.rssi = -127; // Accept any signal strength
  esp_wifi_set_mode(
      WIFI_MODE_STA); // station mode selected - must be set before set_config
  esp_wifi_set_config(
      ESP_IF_WIFI_STA,
      &wifi_config); // setting up configs when event ESP_IF_WIFI_STA
  esp_wifi_start();
  // start connection with configurations provided in funtion
  esp_wifi_connect(); // connect with saved ssid and pass
  ESP_LOGI(MODULE_TAG, "wifi_init_sta finished. SSID:%s  password:%s",
           WIFI_SSID, WIFI_PASS);
}
