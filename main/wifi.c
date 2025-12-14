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
#define WIFI_RETRY_NUM 5
#define MODULE_TAG "WIFI"
static int retry_num = 0;

typedef enum {
  WIFI_DISCONNECTED = 0,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_GOT_IP,
} wifi_state_t;
static wifi_state_t wifi_connection_state = WIFI_DISCONNECTED;
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
    ESP_LOGI(MODULE_TAG, "WiFi lost connection\n");
    if (retry_num < WIFI_RETRY_NUM) {
      esp_wifi_connect();
      retry_num++;
      ESP_LOGI(MODULE_TAG, "Retrying to Connect...\n");
    }
  } else if (event_id == IP_EVENT_STA_GOT_IP) {
    wifi_connection_state = WIFI_GOT_IP;
    ESP_LOGI(MODULE_TAG, "Wifi got IP...\n\n");
  }
}
void wifi_connection() {
  // Initialize NVS - required for WiFi to store configuration
  esp_err_t ret = nvs_flash_init();

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
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
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
