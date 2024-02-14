#include "MqttHelper.h"

String mqtt_client_id = "unset-client-name";
String mqtt_username = "unset-username-name";
String mqtt_password = "unset-password-name";

WiFiClient wlan_client;
MQTTClient mqtt_client(512);
ConnectedHandler connectedHandler;

void connect();

void setupMqtt(String name, String mqtt_host, String username, String password, MQTTClientCallbackSimple messageHandler,  ConnectedHandler conHandler) {
  mqtt_username = username;
  mqtt_password = password;
  mqtt_client_id = name + "-client";
  connectedHandler = conHandler;
  mqtt_client.begin(mqtt_host.c_str(), wlan_client);
  mqtt_client.onMessage(messageHandler);

  connect();
}

void handleMqtt() {
  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    connect();
  }
}

void connect() {
  Serial.print("\nMQTT connecting...");
  int waiting_time = millis();
  while (!mqtt_client.connect(mqtt_client_id.c_str(), mqtt_username.c_str(), mqtt_password.c_str())) {
    Serial.print(".");
    if (millis() - waiting_time > 15000) {
      Serial.println("Connection Failed! Rebooting...");
      delay(500);
      ESP.restart();
    }
    delay(1000);
  }

  Serial.println("\nMQTT connected!");

  connectedHandler();
}

void mqtt_publish(String topic, String message) {
  mqtt_client.publish(topic, message);
}

void mqtt_subscribe(String pattern) {
  mqtt_client.subscribe(pattern);
}

void mqtt_unsubscribe(String pattern) {
  mqtt_client.unsubscribe(pattern);
}
