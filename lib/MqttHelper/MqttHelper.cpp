#include "MqttHelper.h"

String mqtt_client_id = "unset-client-name";
String mqtt_username = "unset-username-name";
String mqtt_password = "unset-password-name";

WiFiClient wlan_client;
MQTTClient mqtt_client(1024, 256);
ConnectedHandler connectedHandler;

unsigned long mqtt_last_connect_attempt = 0;
const unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

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
    unsigned long now = millis();
    if (now - mqtt_last_connect_attempt > MQTT_RETRY_INTERVAL_MS) {
      mqtt_last_connect_attempt = now;
      connect();
    }
  }
}

void connect() {
  if (mqtt_client.connected()) {
    return;
  }

  Serial.print("\nMQTT connecting...");

  if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_username.c_str(), mqtt_password.c_str())) {
    Serial.println("\nMQTT connected!");
    if (connectedHandler != NULL) {
      connectedHandler();
    }
    return;
  }

  Serial.print(".");
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
