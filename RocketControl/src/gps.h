#include <Arduino.h>
#include <TinyGPSPlus.h>

const int pinGpsTX = 6, pinGpsRX = 5;
const int GPS_BAUD = 115200;
#define GPS_SERIAL Serial0

TinyGPSPlus gps;

void gpsInit() {
    GPS_SERIAL.setPins(pinGpsRX, pinGpsTX);
    GPS_SERIAL.begin(GPS_BAUD);
}

uint32_t lastGpsPrint = 0;

void gpsLoop() {
    while (GPS_SERIAL.available()) {
        gps.encode(GPS_SERIAL.read());
    }

    if (millis() - lastGpsPrint > 5000) {
        lastGpsPrint = millis();
        // if (gps.charsProcessed() < 10)
        //     Serial.println(F("[GPS] WARNING: No GPS data.  Check wiring."));

        // Serial.print(F("[GPS] DIAGS      Chars="));
        // Serial.print(gps.charsProcessed());
        // Serial.print(F(" Sentences-with-Fix="));
        // Serial.print(gps.sentencesWithFix());
        // Serial.print(F(" Failed-checksum="));
        // Serial.print(gps.failedChecksum());
        // Serial.print(F(" Passed-checksum="));
        // Serial.println(gps.passedChecksum());
    }
}