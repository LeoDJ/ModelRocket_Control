#pragma once

#include <FS.h>
#include <LittleFS.h>
#include "radio.h"

class TelemetryFS {
    protected: 
    #define FOLDER_NAME     "/telem"
    #define FILE_FORMAT     "%04d.bin"

    public:
    void init() {
        LittleFS.begin(true);
        Serial.printf("LittleFS Free Bytes: %d\n", LittleFS.totalBytes() - LittleFS.usedBytes());

        // listDir(LittleFS, "/", 1);
        openNextTelemFile();
    }

    void printHex(uint8_t* buf, size_t size, size_t viewOffset = 0) {
        if (!viewOffset) {
            printf("           ");
            for(uint8_t i = 0; i < 16; i++) {
                printf("%1X  ", i);
            }
        }
        printf("\n%08X  ", viewOffset);
        for(size_t i = 0; i < size; i++) {
            printf("%02X ", buf[i]);
            if(i % 16 == 15 && i != size-1) {
                printf("\n%08X  ", viewOffset + i + 1);
            }
        }
        // printf("\n");
    }

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
        _telemFile = LittleFS.open(fileToCreate, FILE_APPEND);

        return &_telemFile;
    }

    void close() {
        if (_telemFile) {
            _telemFile.close();
        }
    }

    void flush() {
        if (_telemFile) {
            _telemFile.flush();
        }
    }

    bool write(uint8_t *data, size_t len) {
        if (_telemFile) {
            _telemFile.write(data, len);
            return true;
        }
        return false;
    }

    void format() {
        if (_telemFile) {
            _telemFile.close();
        }

        LittleFS.format();
        openNextTelemFile();
    }

    bool deleteFile(int id) {
        char path[32] = {0};
        snprintf(path, sizeof(path), FOLDER_NAME "/" FILE_FORMAT, id);
        if (LittleFS.exists(path)) {
            return LittleFS.remove(path);
        }
        return false;
    }

    bool hexdump(int id) {
        char path[32] = {0};
        snprintf(path, sizeof(path), FOLDER_NAME "/" FILE_FORMAT, id);
        if (LittleFS.exists(path)) {
            File readFile = LittleFS.open(path, FILE_READ);
            if (readFile) {
                char buf[256];
                size_t readLen;
                do {
                    size_t fileOffset = readFile.position();
                    readLen = readFile.readBytes(buf, sizeof(buf));
                    printHex((uint8_t*)buf, readLen, fileOffset);
                } while (readLen == sizeof(buf));
                readFile.close();
                return true;
            }
        }
        return false;
    }

    protected:
    File _telemFile;
};

class Telemetry {
    protected:
    const int FILE_FLUSH_INTERVAL = 500;

    public:
    // 64 Byte @ 100Hz would last around 156s per MB
    // struct {
    //     uint32_t millis;
    //     int16_t height_dm;
    //     int8_t temp_c;
    //     int8_t padding;
    //     int16_t accel[3];
    //     int16_t gyro[3];
    //     int16_t magnet[3];
    //     // int16_t linearAccel[3];
    //     // int16_t gravity[3];
    //     int16_t rotation[3];
    //     // int16_t geomagRotation[3];
    //     // int16_t gameRotation[3];
    //     uint8_t servoPos[5];
    //     float gpsLat;
    //     float gpsLon;
    //     int16_t gpsAlt;
    // } __attribute__((packed)) dataLog_t;

    enum logEntryDef_type_e : uint8_t {
        T_I8,
        T_U8,
        T_I16,
        T_U16,
        T_I32,
        T_U32,
        T_FLOAT,
        T_I16_VEC3,
        T_U8_VEC4,
        TYPE_COUNT,   // last element
    };

    uint8_t logEntryDef_type_size[TYPE_COUNT] = {
        1, 1, 2, 2, 4, 4, 4, 6, 4
    };

    typedef struct {
        logEntryDef_type_e type;
        const char name[16];
        uint16_t multiplier;
        uint16_t _size, _offset;    // auto generated
    } __attribute__((packed)) logEntryDef_t;

    inline static logEntryDef_t logEntryDef[] = {
        // type         | name (len: 16) | multiplier (optional)
        { T_U32,        "millis",                   },
        { T_I16,        "height",           10,     },
        { T_I8,         "temp_c",                   },
        { T_I16_VEC3,   "accel",                    },
        { T_I16_VEC3,   "gyro",                     },
        { T_I16_VEC3,   "magn",                     },
        { T_I16_VEC3,   "rotation",                 },
        { T_U8_VEC4,    "finServoPos",              },
        { T_U8,         "paraServoPos",             },
        { T_FLOAT,      "gps_lat",                  },
        { T_FLOAT,      "gps_lon",                  },
        { T_I16,        "gps_alt",          10,     },
        { T_U8,         "gps_SV",                   },
    };

    typedef struct {
        size_t headerSize;
        uint16_t numLogEntryDefs;
        logEntryDef_t logEntryDefs[];
    } __attribute__((packed)) flashEntryHeader_t;

    Telemetry() {
        int offset = 0;
        for (int i = 0; i < logEntryDef_num; i++) {
            int size = logEntryDef_type_size[logEntryDef[i].type];
            logEntryDef[i]._size = size;
            logEntryDef[i]._offset = offset;
            offset += size;
        }
        logEntryBufSize = offset;
        logEntryBuf = (uint8_t*)malloc(logEntryBufSize);
    }

    bool set(int idx, void *value) {
        // Check if index is in valid range
        if (idx < 0 || idx >= logEntryDef_num) {
            return false;
        }

        logEntryDef_t logEntry = logEntryDef[idx];
        // int multiplier = logEntry.multiplier;
        // if (multiplier == 0) {
        //     multiplier = 1;
        // }

        // void *valuePtr;
        // switch(logEntry.type) {
        //     case T_I8: {
        //         int8_t val = *(int8_t*)value;  // get value from buffer
        //         val *= multiplier;
        //         valuePtr = &val;    // TODO: check if this is still accessible outside the scope
        //         break;
        //     }
        //     case T_I16: {
        //         int16_t val = *(int16_t*)value;  // get value from buffer
        //         val *= multiplier;
        //         valuePtr = &val;
        //         break;
        //     }
        //     default: {
        //         valuePtr = value;  // use raw value from buffer
        //     }
        // }

        memcpy(logEntryBuf + logEntry._offset, value, logEntry._size);
        return true;
    }

    bool set(int idx, float value) {
        // Check if index is in valid range
        if (idx < 0 || idx >= logEntryDef_num) {
            return false;
        }

        logEntryDef_t logEntry = logEntryDef[idx];

        uint8_t valueBuf[16];
        void *valuePtr;
        int multiplier = logEntry.multiplier;
        if (multiplier == 0) {
            multiplier = 1;
        }

        // Multiply value in place
        value *= multiplier;

        switch(logEntry.type) {
            case T_I8:      *(int8_t*)&valueBuf = value;    break;
            case T_I16:     *(int16_t*)&valueBuf = value;   break;
            case T_I32:     *(int32_t*)&valueBuf = value;   break;
            case T_U8:      *(uint8_t*)&valueBuf = value;   break;
            case T_U16:     *(uint16_t*)&valueBuf = value;  break;
            case T_U32:     *(uint32_t*)&valueBuf = value;  break;
            case T_FLOAT:   *(float*)&valueBuf = value;     break;
            default:        
                Serial.printf("[Telem] Error: No converter for type %d defined.\n", logEntry.type); 
                return false;
        }

        return set(idx, valueBuf); 
    }

    bool set(const char *fieldName, void *value) {
        for (int i = 0; i < logEntryDef_num; i++) {
            if (strcmp(fieldName, logEntryDef[i].name) == 0) {
                return set(i, value);
            }
        }
        return false;
    }

    bool set(const char *fieldName, float value) {
        for (int i = 0; i < logEntryDef_num; i++) {
            if (strcmp(fieldName, logEntryDef[i].name) == 0) {
                return set(i, value);
            }
        }
        return false;
    }


    // void dump()

    bool commit() {
        radio.send(logEntryBuf, logEntryBufSize);
        bool success = fs.write(logEntryBuf, logEntryBufSize);
        if (success) {
            memset(logEntryBuf, 0, logEntryBufSize);

        }
        return success;
    }

    void init(bool receiver = false) {
        fs.init();

        flashEntryHeader_t header = {
            .headerSize = sizeof(flashEntryHeader_t) + sizeof(logEntryDef),
            .numLogEntryDefs = logEntryDef_num,
        };
        fs.write((uint8_t*)&header, sizeof(header));
        fs.write((uint8_t*)&logEntryDef, sizeof(logEntryDef));

        radio.init(receiver);
    }

    void loop() {
        if (millis() - lastTelemFlush >= FILE_FLUSH_INTERVAL) {
            lastTelemFlush = millis();
            fs.flush();
        }
    }


    TelemetryFS fs;

    protected:
    int logEntryBufSize = 0;    // auto generated
    uint8_t *logEntryBuf = nullptr;
    const int logEntryDef_num = sizeof(logEntryDef)/sizeof(logEntryDef[0]);
    uint32_t lastTelemFlush = 0;
};

inline Telemetry telemetry;


