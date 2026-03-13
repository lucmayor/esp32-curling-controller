#include <Arduino.h>
#include <esp_wifi.h>
#include "WiFi.h"
#include <esp_now.h>
#include "esp_timer.h"

#include "curling.h"

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void pairing_call();

// global cmds
Stage state;
uint8_t wide_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// pins
const int BUTTON_ONE = 00;
const int BUTTON_TWO = 00;
const int BUTTON_THR = 00;
const int BUTTON_FOU = 00;

int start_search_time = 0;

void setup()
{
  Serial.begin(9600);
  setCpuFrequencyMhz(80);

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

    // startup pairing
    if (start_search_time == 0)
    {
      start_search_time = time;
    }
    // timeout after 30 seconds
    if ((start_search_time - time) > 30000)
    {
      state = Sleep;
      start_search_time = 0;
      return;
    }

    // send out signal
    // four times per second...
    if ((time % 250) == 0)
    {
      pairing_call();
    }
    break;
  case MainLoop:
    // check against .. also call sleeps here

    break;
  case StartSleep:
    // attach interrupts to wakeup device
    attachInterrupt(digitalPinToInterrupt(BUTTON_ONE), wakeup, RISING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_TWO), wakeup, RISING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_THR), wakeup, RISING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_FOU), wakeup, RISING);

    state = Sleep;

    break;
  case Sleep:
    // sleep is only used when
    // no peers connected
    // and search times out

    // wake up on button interrupt
    break;
  default:
    // should never hit this, sanity check
    Serial.println("ERROR: Controller in unknown state!");
    break;
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if (status == ESP_NOW_SEND_FAIL)
  {
    Serial.print("ERROR SENDING TO: ");
    Serial.println(*mac_addr);
  }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data)
{
  // handling method .. can move out of this and into separate recv_cb binds within the loop
  // this works for now (probably)
  switch (state)
  {
  case TeamChoice:
    ConnectMessage data;
    memcpy(&data, incoming_data, sizeof(data));
    break;
  case MainLoop:
    MainMessage data;
    memcpy(&data, incoming_data, sizeof(data));
    break;
  case Sleep:
    break;
  }
}

void pairing_call()
{
  ConnectMessage data;
  getMac(data.addr);
  esp_err_t res = esp_now_send(wide_addr, (uint8_t *)&data, sizeof(data));

  if (res != ESP_OK)
  {
    Serial.println("ERROR: Sending pairing message failed!");
  }
}

void ARDUINO_ISR_ATTR wakeup()
{
  state = TeamChoice;

  // remove all interrupts
  detachInterrupt(digitalPinToInterrupt(BUTTON_ONE));
  detachInterrupt(digitalPinToInterrupt(BUTTON_TWO));
  detachInterrupt(digitalPinToInterrupt(BUTTON_THR));
  detachInterrupt(digitalPinToInterrupt(BUTTON_FOU));
}