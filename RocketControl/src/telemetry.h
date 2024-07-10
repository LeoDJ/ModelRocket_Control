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

    static void printHex(uint8_t* buf, size_t size, size_t viewOffset = 0) {
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

    File open(int id) {
        char path[32] = {0};
        snprintf(path, sizeof(path), FOLDER_NAME "/" FILE_FORMAT, id);
        if (LittleFS.exists(path)) {
            return LittleFS.open(path, FILE_READ);
        }
        return File();
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
        char name[16];
        float multiplier;
        uint16_t _size, _offset;    // auto generated
    } __attribute__((packed)) logEntryDef_t;

    inline static logEntryDef_t logEntryDef[] = {
        // type         | name (len: 16) | multiplier (optional)
        { T_U32,        "millis",                   },
        { T_I16,        "height",           10,     },
        { T_I8,         "temp_c",                   },
        { T_I16_VEC3,   "accel",            100,    },
        { T_I16_VEC3,   "gyro",             10,     },
        { T_I16_VEC3,   "magn",             100,    },
        { T_I16_VEC3,   "rotation",         10,     },
        { T_U8_VEC4,    "finServoPos",      0.5,    },
        { T_U8,         "paraServoPos",     0.5,    },
        { T_FLOAT,      "gps_lat",                  },
        { T_FLOAT,      "gps_lon",                  },
        { T_I16,        "gps_alt",          10,     },
        { T_U8,         "gps_SV",                   },
    };
    
    const int logEntryDef_num = sizeof(logEntryDef)/sizeof(logEntryDef[0]);
    int logEntryBufSize = 0;    // auto generated

    typedef struct {
        size_t headerSize;  // size of header + logEntryDef[]
        uint16_t numLogEntryDefs;
        // logEntryDef_t logEntryDefs[];
    } __attribute__((packed)) flashEntryHeader_t;

    Telemetry() {
        int offset = 0;
        for (int i = 0; i < logEntryDef_num; i++) {
            int size = logEntryDef_type_size[logEntryDef[i].type];
            logEntryDef[i]._size = size;
            logEntryDef[i]._offset = offset;
            if (logEntryDef[i].multiplier == 0) {
                logEntryDef[i].multiplier = 1;
            }
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

        memcpy(logEntryBuf + logEntry._offset, value, logEntry._size);
        return true;
    }

    bool set(int idx, float val1, float val2 = 0, float val3 = 0, float val4 = 0) {
        // Check if index is in valid range
        if (idx < 0 || idx >= logEntryDef_num) {
            return false;
        }

        logEntryDef_t logEntry = logEntryDef[idx];

        uint8_t valueBuf[16];
        void *valuePtr;
        int multiplier = logEntry.multiplier;

        // Multiply value in place
        val1 *= multiplier;
        val2 *= multiplier;
        val3 *= multiplier;
        val4 *= multiplier;

        switch(logEntry.type) {
            case T_I8:      *(int8_t*)&valueBuf = val1;    break;
            case T_I16:     *(int16_t*)&valueBuf = val1;   break;
            case T_I32:     *(int32_t*)&valueBuf = val1;   break;
            case T_U8:      *(uint8_t*)&valueBuf = val1;   break;
            case T_U16:     *(uint16_t*)&valueBuf = val1;  break;
            case T_U32:     *(uint32_t*)&valueBuf = val1;  break;
            case T_FLOAT:   *(float*)&valueBuf = val1;     break;
            case T_I16_VEC3:
                ((int16_t*)&valueBuf)[0] = val1;
                ((int16_t*)&valueBuf)[1] = val2;
                ((int16_t*)&valueBuf)[2] = val3;
                break;
            case T_U8_VEC4:
                ((uint8_t*)&valueBuf)[0] = val1;
                ((uint8_t*)&valueBuf)[1] = val2;
                ((uint8_t*)&valueBuf)[2] = val3;
                ((uint8_t*)&valueBuf)[3] = val4;
                break;
            default:        
                Serial.printf("[Telem] Error: No converter for type %d defined.\n", logEntry.type); 
                return false;
        }

        return set(idx, valueBuf); 
    }

    bool set(const char *fieldName, void *value) {
        int idx = getIndex(fieldName);
        if (idx >= 0) {
            return set(idx, value);
        }
        return false;
    }

    bool set(const char *fieldName, float val1, float val2 = 0, float val3 = 0, float val4 = 0) {
        int idx = getIndex(fieldName);
        if (idx >= 0) {
            return set(idx, val1, val2, val3, val4);
        }
        return false;
    }

    float get(uint8_t* buf, int idx) {
        // Check if index is in valid range
        if (idx < 0 || idx >= logEntryDef_num) {
            return false;
        }


        logEntryDef_t logEntry = logEntryDef[idx];
        int multiplier = logEntry.multiplier;

        float val = NAN;

        void *bufPtr = buf + logEntry._offset;

        switch(logEntry.type) {
            case T_I8:      val = *(int8_t*)bufPtr;     break;
            case T_I16:     val = *(int16_t*)bufPtr;    break;
            case T_I32:     val = *(int32_t*)bufPtr;    break;
            case T_U8:      val = *(uint8_t*)bufPtr;    break;
            case T_U16:     val = *(uint16_t*)bufPtr;   break;
            case T_U32:     val = *(uint32_t*)bufPtr;   break;
            case T_FLOAT:   val = *(float*)bufPtr;      break;
            default:        
                Serial.printf("[Telem] Error: No converter for type %d defined.\n", logEntry.type); 
                return false;
        }
        // Serial.printf(" %d: %08X, %f\n", idx, *(uint32_t*)bufPtr, val);

        // Multiply value in place
        val /= multiplier;

        return val; 
    }

    float get(uint8_t* buf, const char *fieldName) {
        int idx = getIndex(fieldName);
        if (idx >= 0) {
            return get(buf, idx);
        }
        return NAN;
    }


    void printCsvHeader(logEntryDef_t *entryDefs, size_t num) {
        for (int i = 0; i < num; i++) {
            switch (entryDefs[i].type) {
                case T_I16_VEC3:
                    Serial.printf("%s.x,%s.y,%s.z", entryDefs[i].name, entryDefs[i].name, entryDefs[i].name);
                    break;
                case T_U8_VEC4:
                    Serial.printf("%s.a,%s.b,%s.c,%s.d", entryDefs[i].name, entryDefs[i].name, entryDefs[i].name, entryDefs[i].name);
                    break;
                default:
                    Serial.printf("%s", entryDefs[i].name);                    
                    break;
            }

            if (i < (num - 1)) {
                Serial.printf(",");
            }
        }
        Serial.printf("\n");
    }

    void printCsvRecord(logEntryDef_t *entryDefs, size_t entryNum, const char *recordBuf) {
        for (int i = 0; i < entryNum; i++) {
            float multiplier = entryDefs[i].multiplier;
            if (multiplier == 0) {
                multiplier = 1;
            }

            union {
                uint32_t u32;
                int32_t i32;
                float f32;
                uint8_t u8_4[4];
                int16_t i16_3[3];
            } __attribute__((packed)) val;

            memset(&val, 0, sizeof(val));
            memcpy(&val, recordBuf + entryDefs[i]._offset, entryDefs[i]._size);

            switch (entryDefs[i].type) {
                case T_U8:
                case T_U16:
                case T_U32:
                    if (multiplier <= 1)    Serial.printf("%8u", (uint32_t)(val.u32 / multiplier));
                    else                    Serial.printf("%10g", (val.u32 / multiplier));
                    break;
                case T_I8:
                case T_I16:
                case T_I32:
                    if (multiplier <= 1)    Serial.printf("%8d", (int32_t)(val.i32 / multiplier));
                    else                    Serial.printf("%10g", (val.i32 / multiplier));
                    break;
                case T_FLOAT:   
                    Serial.printf("%10f", (val.f32 / multiplier)); 
                    break;
                case T_I16_VEC3:
                    for (int j = 0; j < 3; j++) {
                        Serial.printf("%8g%s", (val.i16_3[j] / multiplier), j < 2 ? "," : "");
                    }
                    break;
                case T_U8_VEC4:
                    for (int j = 0; j < 4; j++) {
                        Serial.printf("%5g%s", (val.u8_4[j] / multiplier), j < 3 ? "," : "");
                    }
                    break;
            }

            if (i < (entryNum - 1)) {
                Serial.printf(",");
            }
        }
        Serial.printf("\n");
    }

    void dump(int id) {
        File file = fs.open(id);
        if (file) {
            flashEntryHeader_t header;
            file.readBytes((char*)&header, sizeof(header));

            size_t logEntryDefSize = header.headerSize - sizeof(flashEntryHeader_t);

            if (logEntryDefSize / header.numLogEntryDefs != sizeof(logEntryDef_t)) {
                Serial.printf("[Telem] Dump Error: incompatible log entry definition format!\n");
                return;
            }

            logEntryDef_t fileLogEntryDefs[header.numLogEntryDefs];
            file.readBytes((char*)fileLogEntryDefs, logEntryDefSize);
            printCsvHeader(fileLogEntryDefs, header.numLogEntryDefs);

            // calculate metadata of log entries based on the read header.
            int logEntryRecordSize = 0;
            for (int i = 0; i < header.numLogEntryDefs; i++) {
                fileLogEntryDefs[i]._offset = logEntryRecordSize;
                fileLogEntryDefs[i]._size = logEntryDef_type_size[fileLogEntryDefs[i].type];
                logEntryRecordSize += fileLogEntryDefs[i]._size;
            }

            char buf[logEntryRecordSize];
            while (file.available() >= logEntryRecordSize) {
                file.readBytes(buf, logEntryRecordSize);
                printCsvRecord(fileLogEntryDefs, header.numLogEntryDefs, buf);
            }
        }
    }


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
            .numLogEntryDefs = (uint16_t)logEntryDef_num,
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
    uint8_t *logEntryBuf = nullptr;
    uint32_t lastTelemFlush = 0;

    int getIndex(const char *fieldName) {
        for (int i = 0; i < logEntryDef_num; i++) {
            if (strcmp(fieldName, logEntryDef[i].name) == 0) {
                return i;
            }
        }
        return -1;
    }
};

inline Telemetry telemetry;


