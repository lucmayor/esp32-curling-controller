#ifndef CURLING_H_
#define CURLING_H_

#include <Arduino.h>
#include <esp_wifi.h>
#include "WiFi.h"
#include <esp_now.h>

// Controller state enum.
enum Stage
{
    TeamChoice,
    MainLoop,
    Sleep
};

enum Command
{
    LookingPeers, // sent by host, looking for peers
    LookingHost,  //
    Connected,
    Disconnect
};

// pairing system payload.
typedef struct payload
{
    uint8_t command;
    uint8_t addr[6];
} ConnectMessage;

// command payload. no discrimination
typedef struct main_load
{
    uint8_t command;
} MainMessage;

// cached message
typedef struct cache
{
    // Message msg;
    int64_t timestamp;
} CachedMessage;

void getMac(uint8_t mac_addr[]);

void getMac(uint8_t mac_addr)
{
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK)
    {
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    }
    else
    {
        Serial.println("Failed to read MAC address");
    }
}

#endif