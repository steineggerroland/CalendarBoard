#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


void connectWlan(String name, String ssid, String password, String ota_password);

void handleOta();
