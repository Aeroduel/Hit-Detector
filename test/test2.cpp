#include <WiFi.h>

#define BOARD_ID "BOARD1"

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  const char* ssid = "Aeroduel_" BOARD_ID;
  const char* password = "12345678"; // optional, can be empty ""

  // Start Soft AP
  WiFi.softAP(ssid, password);

  Serial.println("Soft AP started!");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP()); // usually 192.168.4.1
}

void loop() {
  // Nothing needed here
}
