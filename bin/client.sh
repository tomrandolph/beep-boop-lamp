source "$(dirname "$0")/../.env"

npx mqtt pub -h $MQTT_HOST -i testing -u $MQTT_USERNAME -P $MQTT_PASSWORD -t esp001/state -C mqtts PULSE
