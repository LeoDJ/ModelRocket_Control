#include <Arduino.h>

#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "telemetry.h"
#include "radio.h"
// #include "console.h"    // needs to be last file to be included


void setup() {

    Serial.begin(115200);
    delay(5000);
    Serial.println("Hello World");

    
    telemetry.init(true);
}

void loop() {
    while (radio.available()) {
        Radio::rcvPacket_t pkt = radio.getPacket();
        Serial.printf("%d, %d\n", pkt.dataLen, *(uint32_t*)pkt.data);
    }
}
