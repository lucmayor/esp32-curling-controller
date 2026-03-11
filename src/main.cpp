#include <Arduino.h>
#include <esp_wifi.h>
#include "WiFi.h"
#include <esp_now.h>
#include "esp_timer.h"

#include "filetypes.h"

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data);
void pairing_call();

// global cmds
Stage state;

void setup()
{
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.channel(1);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ERROR: Initialization of ESP-NOW failed!");
    return;
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  state = TeamChoice;
}

void loop()
{
  switch (state)
  {
  case TeamChoice:
    uint64_t time = esp_timer_get_time() / 1000;

    // send out signal
    if ((time % 250) == 0)
    {
      pairing_call();
    }
    break;
  case MainLoop:
    break;
  case Sleep:
    break;
  default:
    break;
  }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data)
{
  Message data;
  memcpy(&data, incoming_data, sizeof(data));

  // handling method .. can move out of this and into separate recv_cb binds within the loop
  // this works for now (probably)
  switch (state)
  {
    case TeamChoice:
      break;
    case MainLoop:
      break;
    case Sleep:
      break;
  }
}

void pairing_call()
{
  
}