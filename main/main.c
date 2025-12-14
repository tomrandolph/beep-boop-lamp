#include "esp_log.h"
#include "led.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "wifi.h"
#define MODULE_TAG "MAIN"
static void on_mqtt_message_handler(void *handler_args, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  ESP_LOGI(MODULE_TAG, "Received on topic: %.*s\n", event->topic_len,
           event->topic);
  ESP_LOGI(MODULE_TAG, "MQTT received: %.*s", event->data_len, event->data);
  // Convert payload to a null-terminated string
  char payload[64];
  int len = event->data_len;
  if (len >= sizeof(payload))
    len = sizeof(payload) - 1;

  memcpy(payload, event->data, len);
  payload[len] = '\0';

  // Compare
  if (strcmp(payload, "WHITE") == 0) {
    ESP_LOGI(MODULE_TAG, "Setting led state to white");
    set_led_state(STATE_WHITE);
  } else if (strcmp(payload, "OFF") == 0) {
    ESP_LOGI(MODULE_TAG, "Setting led state to off");
    set_led_state(STATE_OFF);
  } else if (strcmp(payload, "CHASE") == 0) {
    ESP_LOGI(MODULE_TAG, "Setting led state to rainbow chase");
    set_led_state(STATE_RAINBOW_CHASE);
  } else if (strcmp(payload, "PULSE") == 0) {
    set_led_state(STATE_PULSE_WAVE);
  }
}
static void on_wifi_connected_handler(void) {
  ESP_LOGI(MODULE_TAG, "WiFi connected");
  start_mqtt_client(on_mqtt_message_handler);
}

void app_main(void) {
  ESP_LOGI(MODULE_TAG, "Starting application");
  // start
  init_led_strip();
  set_led_state(STATE_OFF);
  // start a task for led loop with lower priority than WiFi/MQTT
  // Priority 3 is lower than typical MQTT (5) and WiFi (23) priorities
  xTaskCreate(start_led_loop, "led_loop", 2048, NULL, 3, NULL);

  wifi_connection(on_wifi_connected_handler);
}
