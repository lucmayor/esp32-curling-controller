#include <Arduino.h>
#include <esp_wifi.h>
#include "WiFi.h"
#include <esp_now.h>
#include "esp_timer.h"

#include "curling.h"

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void pairing_call();
void ARDUINO_ISR_ATTR wakeup();
SweepCmds button_click();

// GLOBAL CMDS
// state checks
Stage state;
int start_search_time = 0;
int last_command_timestamp = 0;
int last_heard_timestamp;

// consts
const uint8_t wide_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t loc_addr[6];

esp_now_peer_num_t peers;
int64_t last_heard[] = {-1, -1}; // currently not used, add if time :x

MainMessage m;

// consts. for pins
const int BUTTON_ONE = 00;
const int BUTTON_TWO = 00;
const int BUTTON_THR = 00;
const int BUTTON_FOU = 00;
const int BUTTON_FIV = 00;

void setup()
{
  Serial.begin(9600);
  setCpuFrequencyMhz(80); // slow clock speed

  // start espnow
  WiFi.mode(WIFI_STA);
  WiFi.channel(1);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ERROR: Initialization of ESP-NOW failed!");
    return;
  }

  // save local mac addr
  get_mac(loc_addr);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // set initialization state
  state = TeamChoice;
}

void loop()
{
  uint64_t time = esp_timer_get_time() / 1000;

  switch (state)
  {
  case TeamChoice:

    // startup pairing
    if (start_search_time == 0)
    {
      start_search_time = time;
    }
    // timeout after 30 seconds
    if ((time - start_search_time) > 30000)
    {
      if (peers.total_num > 0)
      {
        state = MainLoop;
      }
      else
      {
        state = Sleep;
      }
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
    SweepCmds cmd = button_click();
    if (cmd != None)
    {
      m.command = cmd;

      // send data
      esp_now_peer_info_t peer;
      if (esp_now_send(NULL, (uint8_t *)&m, sizeof(m)) != ESP_OK)
      {
        Serial.println("ERROR: Failure sending command message.");
      }
    }

    // prepare for sleep

    break;
  case StartSleep:
    // this is IDLE mode, less so an actual sleep
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
  // only raise issue if failure
  if (status == ESP_NOW_SEND_FAIL)
  {
    Serial.print("ERROR SENDING TO: ");
    Serial.println(*mac_addr);
  }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data, int len)
{
  // handling method .. can move out of this and into separate recv_cb binds within the loop
  // this works for now (probably)
  if (len != sizeof(ConnectMessage) && len != sizeof(MainMessage))
  {
    // early return, bad data
    return;
  }

  switch (state)
  {
  case TeamChoice:
    ConnectMessage data;
    memcpy(&data, incoming_data, sizeof(data));

    if (data.command == LookingHost)
    {
      // add to peerlist
      esp_now_peer_info_t peer;
      memcpy(peer.peer_addr, data.addr, 6);
      if (esp_now_add_peer(&peer) == ESP_OK)
      {
        esp_now_get_peer_num(&peers);
        if (peers.total_num == 1)
        {
          last_heard[0] = 0;
        }
        else if (peers.total_num > 2)
        {
          Serial.println("ERROR: More than 2 peers connected!");
        }
        else
        {
          last_heard[1] = 0;
        }
      }
      else
      {
        // display error
        Serial.println("ERROR: Failure adding peer!");
      }
    }
    else
    {
      // debug message
      Serial.print("ERROR: Bad message received. State-found: ");
      Serial.print(data.command);
      Serial.print(". Delivered by: ");
      for (int i = 0; i < 6; i++)
      {
        Serial.print(data.addr[i]);
      }
      Serial.println("");
    }
    break;
  case MainLoop:
    ConnectMessage data;
    memcpy(&data, incoming_data, sizeof(data));

    // commands from
    switch (data.command)
    {
    case Heartbeat:
      break;
    case Disconnect:
      // attempt to remove peer
      // note: does this count wide addr? most other methods don't
      if (esp_now_del_peer(data.addr) == ESP_OK)
      {
        esp_now_get_peer_num(&peers);
        if (peers.total_num == 0)
        {
          state = StartSleep;
        }
      }
      else
      {
        Serial.println("ERROR: Failure when removing peer!");
      }
      break;
    default:
      // sanity check
      break;
    }

    break;
  case Sleep:
    // this should also check against cmd for a heartbeat
    break;
  default:
    break;
  }
}

void pairing_call()
{
  // build pairing message
  ConnectMessage data;
  get_mac(data.addr);
  data.command = LookingPeers;
  // send message widely
  esp_err_t res = esp_now_send(wide_addr, (uint8_t *)&data, sizeof(data));

  if (res != ESP_OK)
  {
    Serial.println("ERROR: Sending pairing message failed!");
  }

  return;
}

void ARDUINO_ISR_ATTR wakeup()
{
  // place back in active searching state
  state = TeamChoice;

  // remove all interrupts
  detachInterrupt(digitalPinToInterrupt(BUTTON_ONE));
  detachInterrupt(digitalPinToInterrupt(BUTTON_TWO));
  detachInterrupt(digitalPinToInterrupt(BUTTON_THR));
  detachInterrupt(digitalPinToInterrupt(BUTTON_FOU));

  return;
}

SweepCmds button_click()
{
  // stop check, as its the most important command to come through
  // milliseconds of difference, but still...
  if (analogRead(BUTTON_ONE) == 4095)
  {
    return Stop;
  }
  else if (analogRead(BUTTON_TWO) == 4095)
  {
    return Hard;
  }
  else if (analogRead(BUTTON_THR) == 4095)
  {
    return Clean;
  }
  else if (analogRead(BUTTON_FOU) == 4095)
  {
    return Left;
  }
  else if (analogRead(BUTTON_FIV) == 4095)
  {
    return Right;
  }
  else
  {
    // early return no pointer
    return None;
  }
}