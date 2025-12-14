#pragma once
typedef enum {
  STATE_OFF = 0,
  STATE_RAINBOW_CHASE,
  STATE_WHITE,
  STATE_PULSE_WAVE,
} led_state_t;
void init_led_strip();
void set_led_state(led_state_t state);
void start_led_loop();
