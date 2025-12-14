#include "esp_log.h"
#include "mqtt.h"
#include "wifi.h"
#define MODULE_TAG "MAIN"

static void on_wifi_connected_handler(void) {
  ESP_LOGI(MODULE_TAG, "WiFi connected");
  start_mqtt_client();
}

void app_main(void) {
  ESP_LOGI(MODULE_TAG, "Starting application");
  wifi_connection(on_wifi_connected_handler);
}
