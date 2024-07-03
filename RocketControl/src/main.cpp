#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
// #include <SPI.h>

#include "imu.h"
#include "bme.h"
#include "telemetry.h"
#include "gps.h"
#include "console.h"    // needs to be last file to be included

const int pinSDA = 17, pinSCL = 18;


const int servoPins[] = {5, 6, 7, 8};
const int servoNum = sizeof(servoPins) / sizeof(servoPins[0]);
Servo* servos[servoNum] = {0};

void setup() {
    sizeof(data_log_t);
    // ESP32PWM::allocateTimer(0);

    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);

    Serial.begin(115200);
    delay(5000);
    Serial.println("Hello World");

    for (int i = 0; i < servoNum; i++) {
        servos[i] = new Servo();
        servos[i]->attach(servoPins[i]);
        servos[i]->write(90);
    }

    Wire.setPins(2, 3);
    
    telemetryInit();
    // bmeInit();
    // imuInit();
    gpsInit();
}

void loop() {
    // bmeLoop();
    // imuLoop();
    gpsLoop();
    telemetryLoop();
    consoleLoop();
}
