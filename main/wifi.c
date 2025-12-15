#include "dns_server.h"
#include "driver/gpio.h"
#include "esp_event.h" // for wifi event
#include "esp_http_server.h"
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

#define WIFI_RETRY_NUM 10
#define WIFI_NS "wifi"
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
void wifi_connection(const char *ssid, const char *pass,
                     void (*on_wifi_connected_callback)(void)) {
  on_wifi_connected_handler = on_wifi_connected_callback;
  retry_num = 0; // Reset retry counter

  esp_netif_create_default_wifi_sta(); // sets up necessary data structs for
                                       // wifi station interface

  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler,
                             NULL); // creating event handler register for wifi
  esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler,
      NULL); // creating event handler register for ip event
  wifi_config_t wifi_config = {0}; // Zero-initialize the structure
  strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
  strncpy((char *)wifi_config.sta.password, pass,
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
  ESP_LOGI(MODULE_TAG, "wifi_init_sta finished. SSID:%s  password:%s", ssid,
           pass);
}

void save_wifi_credentials(const char *ssid, const char *pass) {
  nvs_handle_t nvs;
  nvs_open(WIFI_NS, NVS_READWRITE, &nvs);

  nvs_set_str(nvs, "ssid", ssid);
  nvs_set_str(nvs, "pass", pass);

  uint8_t valid = 1;
  nvs_set_u8(nvs, "valid", valid);

  nvs_commit(nvs);
  nvs_close(nvs);
}

static void start_softap(void) {
  wifi_config_t ap_config = {.ap = {.ssid = "ESP32-Setup",
                                    .ssid_len = 0,
                                    .password = "configureme",
                                    .channel = 1,
                                    .max_connection = 4,
                                    .authmode = WIFI_AUTH_WPA2_PSK}};

  if (strlen((char *)ap_config.ap.password) == 0) {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();
}
esp_err_t captive_portal_redirect(httpd_req_t *req) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

static esp_err_t wifi_post_handler(httpd_req_t *req) {
  char buf[128];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0)
    return ESP_FAIL;
  buf[ret] = 0;

  char ssid[33] = {0};
  char pass[65] = {0};

  httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
  httpd_query_key_value(buf, "pass", pass, sizeof(pass));

  save_wifi_credentials(ssid, pass);

  httpd_resp_sendstr(req, "Saved. Rebooting...");
  esp_restart();
  return ESP_OK;
}
static esp_err_t root_get_handler(httpd_req_t *req) {
  const char *html =
      "<!DOCTYPE html><html><body>"
      "<h2>Wi-Fi Setup</h2>"
      "<form method=\"POST\" action=\"/wifi\">"
      "SSID:<br><input name=\"ssid\"><br>"
      "Password:<br><input name=\"pass\" type=\"password\"><br><br>"
      "<button type=\"submit\">Save</button>"
      "</form></body></html>";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_sendstr(req, html);
  return ESP_OK;
}

static httpd_handle_t start_http_server(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_start(&server, &config);
  httpd_uri_t redirect_uri = {.uri = "/*",
                              .method = HTTP_GET,
                              .handler = captive_portal_redirect,
                              .user_ctx = NULL};

  httpd_register_uri_handler(server, &redirect_uri);
  httpd_uri_t wifi_post = {
      .uri = "/wifi", .method = HTTP_POST, .handler = wifi_post_handler};
  httpd_uri_t root = {
      .uri = "/", .method = HTTP_GET, .handler = root_get_handler};
  httpd_register_uri_handler(server, &root);

  httpd_register_uri_handler(server, &wifi_post);
  return server;
}
void start_wifi_provisioning(void) {
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap(); // IMPORTANT
  start_softap();

  start_http_server();
  // Start DNS hijack LAST

  dns_server_config_t dns_cfg = {
      .num_of_entries = 1,
      .item = {{
          .name = "*",
          .if_key = esp_netif_get_ifkey(ap_netif),
      }},
  };

  start_dns_server(&dns_cfg);
}
bool load_wifi_credentials(char *ssid, size_t ssid_len, char *pass,
                           size_t pass_len) {
  nvs_handle_t nvs;
  uint8_t valid = 0;

  if (nvs_open(WIFI_NS, NVS_READONLY, &nvs) != ESP_OK)
    return false;

  nvs_get_u8(nvs, "valid", &valid);
  if (!valid) {
    nvs_close(nvs);
    return false;
  }

  nvs_get_str(nvs, "ssid", ssid, &ssid_len);
  nvs_get_str(nvs, "pass", pass, &pass_len);

  nvs_close(nvs);
  return true;
}
#define RESET_BTN GPIO_NUM_12

void wifi_reset_button_init(void) {
  gpio_config_t cfg = {
      .pin_bit_mask = 1ULL << RESET_BTN,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
  };
  gpio_config(&cfg);
}
bool wifi_reset_button_held(uint32_t ms) {
  uint32_t elapsed = 0;

  while (gpio_get_level(RESET_BTN) == 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
    elapsed += 10;
    if (elapsed >= ms)
      return true;
  }
  return false;
}
void clear_wifi_credentials(void) {
  nvs_handle_t nvs;
  nvs_open(WIFI_NS, NVS_READWRITE, &nvs);

  uint8_t valid = 0;
  nvs_set_u8(nvs, "valid", valid);

  nvs_commit(nvs);
  nvs_close(nvs);
}
