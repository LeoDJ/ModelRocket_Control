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
    Serial.println("RocketControl Receiver");

    
    telemetry.init(true);
    // for(int i = 0; i < sizeof(telemetry.logEntryDef)/sizeof(telemetry.logEntryDef[0]); i++) {
    //     Serial.println(telemetry.logEntryDef[i].multiplier == 0);
    // }
    telemetry.printCsvHeader(telemetry.logEntryDef, telemetry.logEntryDef_num);
}

void loop() {
    while (radio.available()) {
        Radio::rcvPacket_t pkt = radio.getPacket();
        // TelemetryFS::printHex(pkt.data, pkt.dataLen);
        // Serial.flush();
        // Serial.println();
        // Serial.printf("%d, %d, lat/lon: %f,%f\n", 
        //     pkt.dataLen, 
        //     (uint32_t)telemetry.get(pkt.data, "millis"), 
        //     telemetry.get(pkt.data, "gps_lat"), 
        //     telemetry.get(pkt.data, "gps_lon")
        // );
        if (pkt.dataLen == telemetry.logEntryBufSize)
        telemetry.printCsvRecord(telemetry.logEntryDef, telemetry.logEntryDef_num, (char*)pkt.data);
    }
}
