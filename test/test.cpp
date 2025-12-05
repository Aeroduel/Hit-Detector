#include <WiFi.h>
#include "hiddengems.h" // contains ssid and password

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  server.begin();
  Serial.println("Server started on port 80");
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    Serial.println("Received request: " + request);
    client.flush();

    // send a JSON response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"connected\",\"board\":\"ESP32_1\"}");
    client.stop();
  }
}
