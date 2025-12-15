#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void wifi_connection(const char *ssid, const char *pass,
                     void (*on_wifi_connected_handler)(void));
void wifi_reset_button_init();
bool wifi_reset_button_held(uint16_t ms);
void clear_wifi_credentials();
bool load_wifi_credentials(char *ssid, size_t ssid_size, char *pass,
                           size_t pass_size);
void start_wifi_provisioning();
