#pragma once
#include <stdint.h>
typedef enum {
  STATE_COLOR,
  STATE_RAINBOW_CHASE,
  STATE_PULSE_WAVE,
} led_state_t;
typedef struct {
  led_state_t state;
  uint8_t r;
  uint8_t g;
  uint8_t b;

} led_command_t;
void init_led_strip();
void set_led_cmd(led_command_t command);
void start_led_loop();
