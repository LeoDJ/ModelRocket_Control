#include "FS.h"
#include <LittleFS.h>


// 64 Byte @ 100Hz would last around 156s per MB
struct {
    uint32_t millis;
    int16_t height_dm;
    int8_t temp_c;
    int8_t padding;
    int16_t accel[3];
    int16_t gyro[3];
    int16_t magnet[3];
    // int16_t linearAccel[3];
    // int16_t gravity[3];
    int16_t rotation[3];
    // int16_t geomagRotation[3];
    // int16_t gameRotation[3];
    float gpsLat;
    float gpsLon;
    int16_t gpsAlt;
} __attribute__((packed)) data_log_t;

#define FOLDER_NAME     "/telem"
#define FILE_FORMAT     "%04d.bin"
const int FILE_FLUSH_INTERVAL = 500;
File telemFile;


void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}


int getNextFileID() {
    if (!LittleFS.exists(FOLDER_NAME)) {
        LittleFS.mkdir(FOLDER_NAME);
    }
    File dir = LittleFS.open(FOLDER_NAME);
    int fileNumToCreate = 0;
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            int fileNum = 0;
            int ret = sscanf(file.name(), FILE_FORMAT, &fileNum);
            if (ret != 0 && fileNum >= fileNumToCreate) {
                fileNumToCreate = fileNum + 1;
            }
        }
        file = dir.openNextFile();
    }
    return fileNumToCreate;
}

fs::File *openNextTelemFile() {
    int id = getNextFileID();
    char fileToCreate[32] = {0};
    snprintf(fileToCreate, sizeof(fileToCreate), FOLDER_NAME "/" FILE_FORMAT, id);
    telemFile = LittleFS.open(fileToCreate, FILE_APPEND);
    return &telemFile;
}

void closeTelemFile() {
    if (telemFile) {
        telemFile.close();
    }
}

void telemetryWrite(uint8_t *data, size_t len) {
    if (telemFile) {
        telemFile.write(data, len);
    }
}

void telemetryFormat() {
    if (telemFile) {
        telemFile.close();
    }

    LittleFS.format();
    openNextTelemFile();
}

bool telemetryDeleteFile(int id) {
    char path[32] = {0};
    snprintf(path, sizeof(path), FOLDER_NAME "/" FILE_FORMAT, id);
    if (LittleFS.exists(path)) {
        return LittleFS.remove(path);
    }
    return false;
}

void telemetryInit() {
    LittleFS.begin(true);
    Serial.printf("LittleFS Free Bytes: %d\n", LittleFS.totalBytes() - LittleFS.usedBytes());

    listDir(LittleFS, "/", 1);
    openNextTelemFile();
}

uint32_t lastTelemFlush = 0;

void telemetryLoop() {
    if (millis() - lastTelemFlush >= FILE_FLUSH_INTERVAL) {
        lastTelemFlush = millis();
        uint32_t data = millis();
        telemetryWrite((uint8_t*)&data, sizeof(data));
        if (telemFile) {
            telemFile.flush();
        }
    }
}