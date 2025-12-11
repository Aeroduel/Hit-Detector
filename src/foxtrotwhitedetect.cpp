#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "hiddengems.h"   // ssid, password, PLANE_NAME

// ---------------------------
// CAMERA UART PINS (working)
// ---------------------------
#define CAM_RX 16   // ESP32 receives from H7 TX
#define CAM_TX 17   // ESP32 sends to H7 RX (not required but kept)

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool matchActive = true;   // TEMP: always allow hits so we can test

// ---------------------------
// SEND HIT TO THE PHONE
// ---------------------------
void broadcastHit() {
  ws.textAll("HIT");
  Serial.println("🔥 HIT sent to phone");
}

// ---------------------------
// HANDLE PHONE COMMANDS
// ---------------------------
void handleIncomingMessage(const String& msg) {
  Serial.print("📩 From Phone: ");
  Serial.println(msg);

  if (msg == "MATCH_START") {
    matchActive = true;
  }
  else if (msg == "MATCH_END") {
    matchActive = false;
  }
}

// ---------------------------
// SETUP
// ---------------------------
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, CAM_RX, CAM_TX);

  Serial.println("\n📡 Aeroduel Plane Booting...");
  Serial.print("Plane Name: ");
  Serial.println(PLANE_NAME);

  // --- WiFi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }

  Serial.println("\n✅ Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // --- mDNS name: foxtrotwhite.local ---
  String mdnsName = PLANE_NAME;
  mdnsName.toLowerCase();
  mdnsName.replace(" ", "");

  if (MDNS.begin(mdnsName.c_str())) {
    Serial.print("🌐 mDNS: http://");
    Serial.print(mdnsName);
    Serial.println(".local");
  } else {
    Serial.println("❌ mDNS failed to start");
  }

  // --- /id endpoint for the phone ---
  server.on("/id", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"name\":\"" + String(PLANE_NAME) + "\",";
    json += "\"model\":\"F22\",";
    json += "\"status\":\"ready\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  // --- WebSocket handler ---
  ws.onEvent([](AsyncWebSocket *server,
                AsyncWebSocketClient *client,
                AwsEventType type,
                void *arg,
                uint8_t *data,
                size_t len) 
  {
    if (type == WS_EVT_CONNECT) {
      Serial.println("📱 Phone WebSocket Connected");
    }
    else if (type == WS_EVT_DISCONNECT) {
      Serial.println("📴 Phone Disconnected");
    }
    else if (type == WS_EVT_DATA) {
      String msg;
      for (int i = 0; i < len; i++) msg += (char)data[i];
      handleIncomingMessage(msg);
    }
  });

  server.addHandler(&ws);
  server.begin();
  Serial.println("🌐 Web Server + WS Ready");
}

// ---------------------------
// MAIN LOOP — CAMERA HIT CHECK
// ---------------------------
void loop() {
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();

    if (msg.length() > 0) {
      Serial.print("CAM SAYS: ");
      Serial.println(msg);

      if (msg == "HIT") {
        Serial.println("💥 HIT FROM CAMERA!");

        if (matchActive) {
          broadcastHit();
        } else {
          Serial.println("❌ HIT IGNORED (match inactive)");
        }
      }
    }
  }

  delay(20);
}
