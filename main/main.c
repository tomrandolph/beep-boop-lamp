#include "esp_log.h"
#include "wifi.h"
#define MODULE_TAG "MAIN"
void app_main(void) {
  ESP_LOGI(MODULE_TAG, "Starting application");
  wifi_connection();
}
