source "$(dirname "$0")/../.env"

npx mqtt pub -h $MQTT_BROKER_HOST  -C $MQTT_BROKER_PROTOCOL -i testing -u $MQTT_USERNAME -P $MQTT_PASSWORD -t esp001/state  PULSE
