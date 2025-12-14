#include <stdio.h>

#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD CONFIG_MQTT_PASSWORD
void app_main(void)
{
    printf(MQTT_BROKER_URI);
    printf(MQTT_USERNAME);
    printf(MQTT_PASSWORD);
}
