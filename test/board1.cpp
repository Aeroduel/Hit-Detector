#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include "hiddengems.h" // your WiFi credentials

// -------------------------
// LoRa Pins
// -------------------------
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   26
#define LORA_DIO0  14

// -------------------------
// Indicators
// -------------------------
#define LED_PIN    13
#define BUZ_PIN    12

// -------------------------
// Board Identifier
// -------------------------
#define BOARD_ID "BOARD1"   // change per board

// -------------------------
// LoRa Frequency
// -------------------------
const long RF_FREQ = 433E6;

// -------------------------
// Web server
// -------------------------
WebServer server(80);

// -------------------------
// Lives counter
// -------------------------
int lives = 5;

// -------------------------
// CAMERA UART (H7 → ESP32)
// -------------------------
HardwareSerial CamSerial(2); // UART2 (GPIO16 RX, GPIO17 TX)
#define CAM_RX 16
#define CAM_TX 17
#define CAM_BAUD 115200

// ----------------------------------------------
// LoRa Send Function
// ----------------------------------------------
void sendPacket(const String &type, const String &to) {
  String msg = String("{\"type\":\"") + type +
               "\",\"from\":\"" + BOARD_ID +
               "\",\"to\":\"" + to +
               "\",\"ts\":" + String(millis()) + "}";

  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();

  Serial.println("Sent " + type + " to " + to);

  // Flash LED / buzz
  digitalWrite(LED_PIN, HIGH);
  tone(BUZ_PIN, type == "HIT" ? 2000 : 1600, 100);
  delay(120);
  digitalWrite(LED_PIN, LOW);
}

// -------------------------
// Web Handlers
// -------------------------
void handleSendHit() {
  if (!server.hasArg("target")) {
    server.send(400, "text/plain", "Missing target");
    return;
  }
  String target = server.arg("target");
  sendPacket("HIT", target);
  server.send(200, "application/json", "{\"status\":\"HIT sent\",\"to\":\"" + target + "\"}");
}

void handleGetLives() {
  String json = String("{\"board\":\"") + BOARD_ID +
                "\",\"lives\":" + String(lives) + "}";
  server.send(200, "application/json", json);
}

// -------------------------
// Setup
// -------------------------
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZ_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {}

  // Init CAMERA UART
  CamSerial.begin(CAM_BAUD, SERIAL_8N1, CAM_RX, CAM_TX);
  Serial.println("Camera UART ready on GPIO16/17.");

  // Initialize LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(RF_FREQ)) {
    Serial.println("LoRa init failed.");
    while (1) delay(1000);
  }
  Serial.println("LoRa ready.");

  le
  // Web routes
  server.on("/send-hit", handleSendHit);
  server.on("/get-lives", handleGetLives);
  server.begin();
  Serial.println("Web server running.");
}

// -------------------------
// Loop
// -------------------------
void loop() {
  server.handleClient();

  // ------------------------------------
  // 1. HANDLE CAMERA "HIT"
  // ------------------------------------
  while (CamSerial.available()) {
    String msg = CamSerial.readStringUntil('\n');
    msg.trim();
    if (msg == "HIT") {
      Serial.println("Camera HIT detected!");

      // Visual feedback
      digitalWrite(LED_PIN, HIGH);
      tone(BUZ_PIN, 2000, 100);
      delay(120);
      digitalWrite(LED_PIN, LOW);

      // Send LoRa packet to opponent
      String target = "BOARD2"; // update per your board setup
      sendPacket("HIT", target);
    } else {
      Serial.println("Camera sent: " + msg);
    }
  }

  // ------------------------------------
  // 2. HANDLE LORA PACKETS
  // ------------------------------------
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();

    Serial.println("Received: " + incoming);

    if (incoming.indexOf("\"type\":\"HIT\"") >= 0 &&
        incoming.indexOf("\"to\":\"" BOARD_ID "\"") >= 0) {

      Serial.println("HIT received via LoRa");
      if (lives > 0) lives--;

      String fromID =
        incoming.substring(incoming.indexOf("\"from\":\"") + 8,
        incoming.indexOf("\",\"to\""));

      sendPacket("ACK", fromID);
    }
  }
}

