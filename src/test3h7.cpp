#include <Arduino.h>

#define CAM_RX 16   // ESP32 receives from H7 TX
#define CAM_TX 17   // ESP32 sends to H7 RX (optional)

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32-S3 UART listening for HIT...");

  // Start UART2 on pins 16/17 @ 115200
  Serial2.begin(115200, SERIAL_8N1, CAM_RX, CAM_TX);
}

void loop() {
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();

    if (msg.length() > 0) {
      Serial.print("Received: ");
      Serial.println(msg);

      if (msg == "HIT") {
        Serial.println("🔥 HIT RECEIVED! 🔥");
      }
    }
  }
}
