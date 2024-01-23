#include <WiFiClient.h>
#include <MQTT.h>


void setupMqtt(String name, String mqtt_host, String username, String password, MQTTClientCallbackSimple messageHandler);

void handleMqtt();

void mqtt_publish(String topic, String message);

void mqtt_subscribe(String pattern); 

void mqtt_unsubscribe(String pattern);
