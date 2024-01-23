#include "WlanHelper.h"

void connectWlan(String name, String ssid, String password, String ota_password) {
  WiFi.hostname(name.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(500);
    ESP.restart();
  }
  Serial.println("WLAN Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setPassword(ota_password.c_str());
  ArduinoOTA.setHostname(name.c_str());
  ArduinoOTA.begin();
}

void handleOta() {
  ArduinoOTA.handle();
}
