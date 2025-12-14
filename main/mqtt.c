#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "mqtt_client.h"
#include <stdio.h>             //for basic printf commands
#include <string.h>            //for handling strings
#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h"        //esp_init funtions esp_err_t
#include "esp_wifi.h"          //esp_wifi_init functions and wifi operations
#include "esp_log.h"           //for showing logs
#include "esp_event.h"         //for wifi event
#include "nvs_flash.h"         //non volatile storage
#include "lwip/err.h"          //light weight ip packets error handling
#include "lwip/sys.h"          //system applications for light weight ip apps
#define LED_GPIO 2
static int count = 0;
static const char *TAG = "mqtt";
static int retry_num = 0;
typedef enum
{
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_GOT_IP,
} wifi_state_t;

typedef enum
{
    LIGHT_NOT_SPECIFIED = 0,
    LIGHT_ON,
    LIGHT_OFF,
    LIGHT_BLINK,

} requested_light_state_t;

static requested_light_state_t requested_state = LIGHT_NOT_SPECIFIED;
static wifi_state_t wifi_connection_state = WIFI_DISCONNECTED;
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
    {
        printf("WIFI CONNECTING....\n");
        wifi_connection_state = WIFI_CONNECTING;
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_connection_state = WIFI_CONNECTED;

        printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connection_state = WIFI_DISCONNECTED;
        printf("WiFi lost connection\n");
        if (retry_num < 5)
        {
            esp_wifi_connect();
            retry_num++;
            printf("Retrying to Connect...\n");
        }
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connection_state = WIFI_GOT_IP;
        printf("Wifi got IP...\n\n");
    }
}
void wifi_connection()
{
    esp_netif_init();                                                                    // network interdace initialization
    esp_event_loop_create_default();                                                     // responsible for handling and dispatching events
    esp_netif_create_default_wifi_sta();                                                 // sets up necessary data structs for wifi station interface
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();                     // sets up wifi wifi_init_config struct with default values
    esp_wifi_init(&wifi_initiation);                                                     // wifi initialised with dafault wifi_initiation
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);  // creating event handler register for wifi
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL); // creating event handler register for ip event
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config); // setting up configs when event ESP_IF_WIFI_STA
    esp_wifi_start();
    // start connection with configurations provided in funtion
    esp_wifi_set_mode(WIFI_MODE_STA); // station mode selected
    esp_wifi_connect();               // connect with saved ssid and pass
    printf("wifi_init_softap finished. SSID:%s  password:%s", WIFI_SSID, WIFI_PASS);
}

void configure_led()
{
    gpio_reset_pin(LED_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

void blink_led()
{

    bool state = 0;
    switch (wifi_connection_state)
    {
    case WIFI_CONNECTED:
        state = (bool)(count % 4);
        break;
    case WIFI_CONNECTING:
        state = (bool)(count % 2);
        break;
    case WIFI_DISCONNECTED:
        state = 0;
        break;
    case WIFI_GOT_IP:
        state = 1;
        break;
    }
    switch (requested_state)
    {
    case LIGHT_NOT_SPECIFIED:
        break;
    case LIGHT_OFF:
        state = 0;
        break;
    case LIGHT_ON:
        state = 1;
        break;
    case LIGHT_BLINK:
        state = count % 2;
        break;
    }

    gpio_set_level(LED_GPIO, state);
}

const char *BROKER_URI = "mqtts://e8497b20de6742e9a7051fce5944cdc4.s1.eu.hivemq.cloud:8883";

static bool mqtt_connected = false;

static void
mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected!");
        mqtt_connected = true;
        printf("MQTT connected, subscribing...\n");
        esp_mqtt_client_subscribe(event->client, "esp001/state", 1);
        esp_mqtt_client_publish(client, "devices/connect", "ack!", 0, 0, 0);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected");
        mqtt_connected = false;
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Received on topic: %.*s\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "MQTT received: %.*s", event->data_len, event->data);
        // Convert payload to a null-terminated string
        char payload[64];
        int len = event->data_len;
        if (len >= sizeof(payload))
            len = sizeof(payload) - 1;

        memcpy(payload, event->data, len);
        payload[len] = '\0';

        // Compare
        if (strcmp(payload, "ON") == 0)
        {
            requested_state = LIGHT_ON;
            ESP_LOGI(TAG, "Flag set to TRUE");
        }
        else if (strcmp(payload, "OFF") == 0)
        {
            requested_state = LIGHT_OFF;
            ESP_LOGI(TAG, "Flag set to FALSE");
        }
        else if (strcmp(payload, "BLINK") == 0)
        {
            requested_state = LIGHT_BLINK;
            ESP_LOGI(TAG, "Flag set to FALSE");
        }
        break;
    default:
        break;
    }
}

void app_main(void)
{
    configure_led();
    nvs_flash_init(); // this is important in wifi case to store configurations , code will not work if this is not added
    wifi_connection();
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = USERNAME,
        .credentials.authentication.password = PASSWORD,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    bool should_start_mqtt_client = true;
    while (1)
    {
        blink_led();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "%d", wifi_connection_state);

        if (wifi_connection_state == WIFI_GOT_IP)
        {

            if (should_start_mqtt_client)
            {
                ESP_LOGI(TAG, "starting mqtt");
                esp_mqtt_client_start(client);
                should_start_mqtt_client = false;
            }

            // if (mqtt_connected)
            // {
            //     char payload[12];
            //     snprintf(payload, sizeof(payload), "%d", count);

            // esp_mqtt_client_publish(client, "test/topic", payload, 0, 0, 0);
            //     ESP_LOGI(TAG, "published %d", payload);
            // }

            count += 1;
        }
        else
        {
            if (!should_start_mqtt_client)
            {

                ESP_LOGI(TAG, "will restart mqtt");
            }
            should_start_mqtt_client = true;
        }
    }
}
