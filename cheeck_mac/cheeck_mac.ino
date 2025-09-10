#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Put ESP32 into Station mode
  WiFi.mode(WIFI_STA);
  delay(100);

  Serial.println("ESP32 MAC Address:");
  Serial.println(WiFi.macAddress());
}

void loop() {
}
