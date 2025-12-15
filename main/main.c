#include "esp_log.h"
#include "led.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "wifi.h"
#define MODULE_TAG "MAIN"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline int hex_nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

bool parse_rgb24(const char *data, size_t len, uint8_t *r, uint8_t *g,
                 uint8_t *b) {

  if (!data || !r || !g || !b) {
    ESP_LOGI("RGB_PARSE", "Missing params");
    return false;
  }

  // Must be exactly "#RRGGBB"
  if (len != 7) {
    ESP_LOGI("RGB_PARSE", "Len was %d, not 7", len);
    return false;
  }
  if (data[0] != '#') {
    ESP_LOGI("RGB_PARSE", "Data started with %s, not #", data[0]);
    return false;
  }

  uint32_t value = 0;

  for (int i = 1; i < 7; i++) {
    int nibble = hex_nibble(data[i]);
    if (nibble < 0) {
      return false;
    }
    value = (value << 4) | (uint32_t)nibble;
  }

  *r = (value >> 16) & 0xFF;
  *g = (value >> 8) & 0xFF;
  *b = value & 0xFF;

  return true;
}

#define PREFIX_LEN (sizeof(STRING_LITERAL) - 1)
#define COLOR_MSG "COLOR#"
#define COLOR_MSG_PREFIX_LEN sizeof(COLOR_MSG) - 1
#define PULSE_MSG "PULSE#"
#define PULSE_MSG_PREFIX_LEN sizeof(PULSE_MSG) - 1
#define CHASE_MSG "CHASE"
#define CHASE_MSG_PREFIX_LEN sizeof(CHASE_MSG) - 1

static void on_mqtt_message_handler(void *handler_args, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  ESP_LOGI(MODULE_TAG, "Received on topic: %.*s", event->topic_len,
           event->topic);
  ESP_LOGI(MODULE_TAG, "MQTT received: %.*s", event->data_len, event->data);

  // We will NOT assume null-termination in logic
  const char *data = event->data;
  size_t len = event->data_len;

  uint8_t r, g, b;

  // ---------- COLOR#RRGGBB ----------
  if (len > COLOR_MSG_PREFIX_LEN &&
      memcmp(data, COLOR_MSG, COLOR_MSG_PREFIX_LEN) == 0) {
    size_t offset = COLOR_MSG_PREFIX_LEN - 1;
    const char *data_start = data + offset;
    size_t data_len = len - offset;
    ESP_LOGD(MODULE_TAG, "RGB segment received using offset %d: %.*s", offset,
             data_len, data_start);

    if (parse_rgb24(data_start, data_len, &r, &g, &b)) {
      ESP_LOGI(MODULE_TAG, "ON color: #%02X%02X%02X", r, g, b);
      led_command_t cmd = {STATE_COLOR, r, g, b};

      set_led_cmd(cmd);
    } else {
      ESP_LOGW(MODULE_TAG, "Invalid ON color payload");
    }
    return;
  }

  // ---------- PULSE#RRGGBB ----------
  if (len > PULSE_MSG_PREFIX_LEN &&
      memcmp(data, PULSE_MSG, PULSE_MSG_PREFIX_LEN) == 0) {
    size_t offset = COLOR_MSG_PREFIX_LEN - 1;
    const char *data_start = data + offset;
    size_t data_len = len - offset;
    ESP_LOGD(MODULE_TAG, "RGB segment received using offset %d: %.*s", offset,
             data_len, data_start);

    if (parse_rgb24(data_start, data_len, &r, &g, &b)) {
      ESP_LOGI(MODULE_TAG, "PULSE color: #%02X%02X%02X", r, g, b);
      led_command_t cmd = {STATE_PULSE_WAVE, r, g, b};
      set_led_cmd(cmd);
    } else {
      ESP_LOGW(MODULE_TAG, "Invalid PULSE color payload");
    }
    return;
  }

  // ---------- CHASE ----------
  if (len == 5 && memcmp(data, "CHASE", 5) == 0) {
    ESP_LOGI(MODULE_TAG, "Setting LED CHASE");
    led_command_t cmd = {STATE_RAINBOW_CHASE, 0, 0, 0};
    set_led_cmd(cmd);
    return;
  }

  ESP_LOGW(MODULE_TAG, "Unknown MQTT command");
}

static void on_wifi_connected_handler(void) {
  ESP_LOGI(MODULE_TAG, "WiFi connected");
  start_mqtt_client(on_mqtt_message_handler);
}

void app_main(void) {
  ESP_LOGI(MODULE_TAG, "Starting application");
  // start
  init_led_strip();
  // start a task for led loop with lower priority than WiFi/MQTT
  // Priority 3 is lower than typical MQTT (5) and WiFi (23) priorities
  xTaskCreate(start_led_loop, "led_loop", 2048, NULL, 3, NULL);

  wifi_connection(on_wifi_connected_handler);
}
