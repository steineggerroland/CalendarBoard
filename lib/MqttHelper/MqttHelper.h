#pragma once
#include <WiFiClient.h>
#include <MQTT.h>

typedef void (*ConnectedHandler)();


void setupMqtt(String name, String mqtt_host, String username, String password,
               MQTTClientCallbackSimple messageHandler, ConnectedHandler connectedHandler);

void handleMqtt();

bool mqtt_publish(String topic, String message);

bool mqtt_subscribe(String pattern);

bool mqtt_unsubscribe(String pattern);

bool mqtt_connected();
void mqtt_disconnect();
