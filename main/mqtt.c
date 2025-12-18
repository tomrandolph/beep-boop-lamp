#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>
#define LED_GPIO 2

#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD CONFIG_MQTT_PASSWORD

#define MODULE_TAG "MQTT"

static bool mqtt_connected = false;

static void mqtt_connection_event_handler(void *handler_args,
                                          esp_event_base_t base,
                                          int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;

  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(MODULE_TAG, "MQTT connected!");
    mqtt_connected = true;
    printf("MQTT connected, subscribing...\n");
    esp_mqtt_client_subscribe(event->client, "esp001/state", 1);
    esp_mqtt_client_publish(client, "devices/connect", "ack!", 0, 0, 0);

    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(MODULE_TAG, "MQTT disconnected");
    mqtt_connected = false;
    break;
  default:
    break;
  }
}
static esp_mqtt_client_handle_t client;

void start_mqtt_client(esp_event_handler_t event_handler) {

  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = MQTT_BROKER_URI,
      .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
      .credentials.username = MQTT_USERNAME,
      .credentials.authentication.password = MQTT_PASSWORD,
  };

  client = esp_mqtt_client_init(&mqtt_cfg);
  if (client == NULL) {
    ESP_LOGE(MODULE_TAG, "Failed to initialize MQTT client");
    return;
  }
  esp_mqtt_client_register_event(client, MQTT_EVENT_DATA, event_handler, NULL);
  esp_mqtt_client_register_event(client, -1, mqtt_connection_event_handler,
                                 NULL);
  ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}

void stop_mqtt_client(void) {
  if (!client) {
    ESP_LOGI(MODULE_TAG, "MQTT client not initialized");
    return;
  }
  esp_mqtt_client_stop(client);
}
