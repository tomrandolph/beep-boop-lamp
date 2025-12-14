/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "led.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip_encoder.h"
#include <string.h>

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
#define RMT_LED_STRIP_GPIO_NUM 13

#define EXAMPLE_LED_NUMBERS 12
#define EXAMPLE_CHASE_SPEED_MS 10

static const char *TAG = "led_control";

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb8_t;

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r,
                       uint32_t *g, uint32_t *b) {
  h %= 360; // h -> [0,360]
  uint32_t rgb_max = v * 2.55f;
  uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

  uint32_t i = h / 60;
  uint32_t diff = h % 60;

  // RGB adjustment amount by hue
  uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

  switch (i) {
  case 0:
    *r = rgb_max;
    *g = rgb_min + rgb_adj;
    *b = rgb_min;
    break;
  case 1:
    *r = rgb_max - rgb_adj;
    *g = rgb_max;
    *b = rgb_min;
    break;
  case 2:
    *r = rgb_min;
    *g = rgb_max;
    *b = rgb_min + rgb_adj;
    break;
  case 3:
    *r = rgb_min;
    *g = rgb_max - rgb_adj;
    *b = rgb_max;
    break;
  case 4:
    *r = rgb_min + rgb_adj;
    *g = rgb_min;
    *b = rgb_max;
    break;
  default:
    *r = rgb_max;
    *g = rgb_min;
    *b = rgb_max - rgb_adj;
    break;
  }
}

static rmt_encoder_handle_t led_encoder = NULL;
static rmt_channel_handle_t led_chan = NULL;

void init_led_strip(void) {

  ESP_LOGI(TAG, "Create RMT TX channel");
  rmt_tx_channel_config_t tx_chan_config = {
      .gpio_num = RMT_LED_STRIP_GPIO_NUM,
      .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
      .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
      .mem_block_symbols =
          64, // increase the block size can make the LED flicker less
      .trans_queue_depth = 4, // set the number of transactions that can be
                              // pending in the background
  };
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

  ESP_LOGI(TAG, "Install led strip encoder");
  led_strip_encoder_config_t encoder_config = {
      .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

  ESP_LOGI(TAG, "Enable RMT TX channel");
  ESP_ERROR_CHECK(rmt_enable(led_chan));
}

static led_state_t led_state = STATE_OFF;

void set_led_state(led_state_t state) {
  // could use queue to be reaaally careful
  led_state = state;
}
void start_led_loop() {
  uint32_t red = 0;
  uint32_t green = 0;
  uint32_t blue = 0;
  uint16_t hue = 0;
  uint16_t start_rgb = 0;
  rmt_transmit_config_t tx_config = {
      .loop_count = 0, // no transfer loop
  };
  while (1) {
    switch (led_state) {
    case STATE_RAINBOW_CHASE:

      for (int i = 0; i < 3; i++) {
        for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3) {
          // Build RGB pixels
          hue = j * 360 / EXAMPLE_LED_NUMBERS + start_rgb;
          led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
          led_strip_pixels[j * 3 + 0] = green;
          led_strip_pixels[j * 3 + 1] = blue;
          led_strip_pixels[j * 3 + 2] = red;
        }
        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels,
                                     sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels,
                                     sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_CHASE_SPEED_MS));
      }
      start_rgb += 60;
      break;
    case STATE_OFF:
      for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {

        led_strip_pixels[i * 3 + 1] = 0;
        led_strip_pixels[i * 3 + 2] = 0;
        led_strip_pixels[i * 3 + 3] = 0;
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels,
                                     sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
      }
      break;
    case STATE_WHITE:
      for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {

        led_strip_pixels[i * 3 + 1] = 127;
        led_strip_pixels[i * 3 + 2] = 127;
        led_strip_pixels[i * 3 + 3] = 127;
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels,
                                     sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
      }
      break;
    }
  }
}
