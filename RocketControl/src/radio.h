#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <queue>

class Radio {
    protected:
    const int CHANNEL = 4;
    const uint8_t ADDRESS[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    public:
    typedef struct {
        uint8_t mac[6];
        uint8_t data[256];
        size_t dataLen;
    } rcvPacket_t;

    void init(bool receiver = false) {
        Serial.printf("[Radio] MAC Address: %s\n", WiFi.macAddress().c_str());

        // Set device as a Wi-Fi Station
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(); // we do not actually want to connect to a WiFi network

        ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));
        
        // Set Long Range Mode
        esp_wifi_set_protocol( WIFI_IF_STA , WIFI_PROTOCOL_LR); 

        // Initialize ESP-NOW
        esp_err_t result = esp_now_init();
        Serial.printf("[Radio] ESPNow init: %d\n", result);


        if (receiver) {
            esp_now_register_recv_cb(OnDataRecv);
        }
        else {  // Transmitter
            // Setup peer info
            memcpy(_peer.peer_addr, ADDRESS, sizeof(_peer.peer_addr));
            _peer.channel = CHANNEL;
            _peer.encrypt = false;

            // Add peer
            if (esp_now_add_peer(&_peer) != ESP_OK) {
                Serial.println("Failed to add peer");
                return;
            }
        }
    }

    bool send(uint8_t *buf, size_t len) {
        esp_err_t result = esp_now_send(ADDRESS, buf, len);
        if (result != ESP_OK) {
            Serial.printf("[Radio] Send failure: %X\n", result);
            return false;
        }
        return true;
    }
    
    int available() {
        return _rcvQueue.size();
    }

    rcvPacket_t getPacket() {
        if (_rcvQueue.size() > 0) {
            rcvPacket_t pkt = _rcvQueue.front();    // get oldest element
            _rcvQueue.pop();                        // remove element from queue
            return pkt;
        }
        rcvPacket_t nullPacket = {0};
        return nullPacket;
    }

    static void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
        rcvPacket_t pkt = {0};
        memcpy(pkt.mac, mac, sizeof(pkt.mac));
        memcpy(pkt.data, incomingData, min(len, (int)sizeof(rcvPacket_t::data)));
        pkt.dataLen = len;
        
        // workaround needed for C-type callback (as long as only one class instance exists)
        extern Radio radio;
        radio._rcvQueue.push(pkt);
    }

    std::queue<rcvPacket_t> _rcvQueue;

    protected:
    esp_now_peer_info_t _peer;


};

inline Radio radio;