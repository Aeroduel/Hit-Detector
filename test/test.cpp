#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "hiddengems.h"

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
#define BOARD_ID "BOARD1"

// -------------------------
// LoRa Frequency
// -------------------------
const long RF_FREQ = 433E6;

// -------------------------
// Web server + WebSocket
// -------------------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// -------------------------
// Camera UART
// -------------------------
HardwareSerial CamSerial(2); // UART2 (GPIO16 RX, GPIO17 TX)
#define CAM_RX 16
#define CAM_TX 17
#define CAM_BAUD 115200

// -------------------------
// Plane struct
// -------------------------
struct Plane {
  String planeId;
  String userId;
  String authToken;
  bool isOnline;
  int lives;
};

#define MAX_PLANES 5
Plane planes[MAX_PLANES];
int planeCount = 0;

// -------------------------
// Forward declarations
// -------------------------
void handleRegister(AsyncWebServerRequest *request);
void handleHit(AsyncWebServerRequest *request);
void handleFire(AsyncWebServerRequest *request);
void handleMatch(AsyncWebServerRequest *request);
void handlePlanes(AsyncWebServerRequest *request);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len);

// -------------------------
// Helper functions
// -------------------------
Plane* findPlaneById(const String &id) {
  for (int i = 0; i < planeCount; i++) {
    if (planes[i].planeId == id) return &planes[i];
  }
  return nullptr;
}

Plane* findPlaneByToken(const String &token) {
  for (int i = 0; i < planeCount; i++) {
    if (planes[i].authToken == token) return &planes[i];
  }
  return nullptr;
}

void broadcastPlaneUpdate() {
  DynamicJsonDocument doc(512);
  for (int i = 0; i < planeCount; i++) {
    doc["planes"][i]["planeId"] = planes[i].planeId;
    doc["planes"][i]["isOnline"] = planes[i].isOnline;
    doc["planes"][i]["lives"] = planes[i].lives;
  }
  String output;
  serializeJson(doc, output);

  ws.textAll(output);
}

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

  digitalWrite(LED_PIN, HIGH);
  tone(BUZ_PIN, type == "HIT" ? 2000 : 1600, 100);
  delay(120);
  digitalWrite(LED_PIN, LOW);
}

// -------------------------
// Web server handlers
// -------------------------
void handleRegister(AsyncWebServerRequest *request) {
  if (!request->hasParam("planeId", true) || !request->hasParam("userId", true)) {
    request->send(400, "application/json", "{\"error\":\"Missing planeId or userId\"}");
    return;
  }

  String planeId = request->getParam("planeId", true)->value();
  String userId = request->getParam("userId", true)->value();

  Plane* existing = findPlaneById(planeId);
  if (existing != nullptr) {
    existing->isOnline = true;
    request->send(200, "application/json", "{\"authToken\":\"" + existing->authToken + "\"}");
    broadcastPlaneUpdate();
    return;
  }

  if (planeCount >= MAX_PLANES) {
    request->send(400, "application/json", "{\"error\":\"Max planes reached\"}");
    return;
  }

  String token = planeId + "-" + String(random(1000, 9999));
  planes[planeCount++] = {planeId, userId, token, true, 5};

  request->send(200, "application/json", "{\"authToken\":\"" + token + "\"}");
  Serial.println("Registered plane: " + planeId + " token: " + token);
  broadcastPlaneUpdate();
}

void handleHit(AsyncWebServerRequest *request) {
  if (!request->hasParam("authToken", true) || !request->hasParam("targetId", true)) {
    request->send(400, "application/json", "{\"error\":\"Missing authToken or targetId\"}");
    return;
  }

  String token = request->getParam("authToken", true)->value();
  String targetId = request->getParam("targetId", true)->value();

  Plane* shooter = findPlaneByToken(token);
  if (!shooter) {
    request->send(401, "application/json", "{\"error\":\"Invalid authToken\"}");
    return;
  }

  Plane* target = findPlaneById(targetId);
  if (!target) {
    request->send(404, "application/json", "{\"error\":\"Target not found\"}");
    return;
  }

  sendPacket("HIT", targetId);
  request->send(200, "application/json", "{\"status\":\"Hit sent\",\"to\":\"" + targetId + "\"}");
}

void handleFire(AsyncWebServerRequest *request) {
  if (!request->hasParam("planeId", true) || !request->hasParam("targetId", true)) {
    request->send(400, "application/json", "{\"error\":\"Missing planeId or targetId\"}");
    return;
  }

  String plane = request->getParam("planeId", true)->value();
  String target = request->getParam("targetId", true)->value();

  sendPacket("HIT", target);
  request->send(418, "application/json", "{\"error\":\"I'm a teapot\"}");
}

void handleMatch(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  doc["board"] = BOARD_ID;
  for (int i = 0; i < planeCount; i++) {
    doc["planes"][i]["planeId"] = planes[i].planeId;
    doc["planes"][i]["lives"] = planes[i].lives;
    doc["planes"][i]["isOnline"] = planes[i].isOnline;
  }
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

void handlePlanes(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  for (int i = 0; i < planeCount; i++) {
    doc[i]["planeId"] = planes[i].planeId;
    doc[i]["isOnline"] = planes[i].isOnline;
    doc[i]["lives"] = planes[i].lives;
  }
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

// -------------------------
// WebSocket callback
// -------------------------
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client connected: %u\n", client->id());
    broadcastPlaneUpdate();
  }
}

// -------------------------
// Setup
// -------------------------
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZ_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {}

  CamSerial.begin(CAM_BAUD, SERIAL_8N1, CAM_RX, CAM_TX);
  Serial.println("Camera UART ready.");

  // Initialize LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(RF_FREQ)) {
    Serial.println("LoRa init failed.");
    while (1) delay(1000);
  }
  Serial.println("LoRa ready.");

  // -------------------------
  // Soft AP Wi-Fi setup
  // -------------------------
  const char* apSSID = "Aeroduel_" BOARD_ID;
  const char* apPassword = "12345678"; // optional
  WiFi.softAP(apSSID, apPassword);
  Serial.print("Soft AP IP: ");
  Serial.println(WiFi.softAPIP());

  // -------------------------
  // Web routes
  // -------------------------
  server.on("/api/register", HTTP_POST, handleRegister);
  server.on("/api/hit", HTTP_POST, handleHit);
  server.on("/api/fire", HTTP_POST, handleFire);
  server.on("/api/match", HTTP_GET, handleMatch);
  server.on("/api/planes", HTTP_GET, handlePlanes);

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Web server + WebSocket running.");
}

// -------------------------
// Loop
// -------------------------
void loop() {
  ws.cleanupClients();

  // Camera HITs
  while (CamSerial.available()) {
    String msg = CamSerial.readStringUntil('\n');
    msg.trim();
    if (msg == "HIT") {
      Serial.println("Camera HIT detected!");
      digitalWrite(LED_PIN, HIGH);
      tone(BUZ_PIN, 2000, 100);
      delay(120);
      digitalWrite(LED_PIN, LOW);

      for (int i = 0; i < planeCount; i++) {
        if (planes[i].planeId != BOARD_ID && planes[i].isOnline) {
          sendPacket("HIT", planes[i].planeId);
          break;
        }
      }
      broadcastPlaneUpdate();
    }
  }

  // LoRa packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    Serial.println("Received: " + incoming);

    if (incoming.indexOf("\"type\":\"HIT\"") >= 0 &&
        incoming.indexOf("\"to\":\"" BOARD_ID "\"") >= 0) {

      Plane* self = findPlaneById(BOARD_ID);
      if (self && self->lives > 0) self->lives--;

      String fromID = incoming.substring(incoming.indexOf("\"from\":\"") + 8,
                                        incoming.indexOf("\",\"to\""));
      sendPacket("ACK", fromID);
      broadcastPlaneUpdate();
    }
  }
}
