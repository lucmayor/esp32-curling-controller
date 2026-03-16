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
    StartSleep,
    Sleep
};

enum Command
{
    LookingPeers, // sent by host, looking for peers
    LookingHost,  // sent by peer, attaching to host
    Connected,    // sent by host, connected confirmation.
    Heartbeat,    // sent by both
    Disconnect
};

enum SweepCmds
{
    Hard,
    Clean,
    Line,
    Curl,
    Stop
};

// pairing system payload.
typedef struct payload
{
    Command command;
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

void get_mac(uint8_t *mac_addr);

void get_mac(uint8_t *mac_addr)
{
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
    if (ret != ESP_OK)
    {
      Serial.println("Failed to read MAC address");
    }
}

#endif