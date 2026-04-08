#include <Arduino.h>
#include <esp_wifi.h>
#include "WiFi.h"
#include <esp_now.h>
#include "esp_timer.h"

#include "curling.h"

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void pairing_call();
void wakeup_check();
SweepCmds button_click();

// GLOBAL CMDS
// state checks
Stage state;
int start_search_time = 0;
int last_search_time = 0;
int last_command_timestamp = 0;
int last_heard_timestamp;

// consts
const uint8_t wide_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t loc_addr[6];

esp_now_peer_num_t peers;
int64_t last_heard[] = {-1, -1}; // currently not used, add if time :x

MainMessage m;

// consts. for pins
const int BUTTON_ONE = 25;
const int BUTTON_TWO = 26; 
const int BUTTON_THR = 27;
const int BUTTON_FOU = 21;
const int BUTTON_FIV = 19;

void setup()
{
  Serial.begin(9600);
  setCpuFrequencyMhz(80); // slow clock speed

  // start espnow
  WiFi.mode(WIFI_STA);
  WiFi.channel(1);

  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: Initialization of ESP-NOW failed!");
    return;
  }

  esp_now_peer_info_t wide_peer = {};
  memcpy(wide_peer.peer_addr, wide_addr, 6);
  wide_peer.channel = 1;
  wide_peer.encrypt = false;
  if (esp_now_add_peer(&wide_peer) != ESP_OK) {
    Serial.println("ERROR: Initialization of WIDE PEER failed!");
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

  switch (state) {
    case TeamChoice: {
      // startup pairing
      if (start_search_time == 0) {
        Serial.println("STARTING");
        start_search_time = time;
      }

      // timeout after 30 seconds (set to 10s for testing)
      if ((time - start_search_time) > 10000) {
        if (peers.total_num > 0) {
          Serial.println("TIMEOUT: MAIN");
          state = MainLoop;
        } else {
          Serial.println("TIMEOUT: SLEEP");
          state = Sleep;
        }
        start_search_time = 0;
        return;
      }

      // send out signal
      // four times per second...
      if ((time - last_search_time) > 250) {
        pairing_call();
        last_search_time = time;
      }

      break;
    }
    case MainLoop: {
      // check against command .. also call sleeps here
      SweepCmds cmd = button_click();
      if (cmd != None) {
        // sending message
        Serial.print("SUCCESS: Sending message ");
        Serial.println(cmd);

        m.command = (uint8_t) cmd;

        // send data
        if (esp_now_send(NULL, (uint8_t *)&m, sizeof(m)) != ESP_OK) {
          Serial.println("ERROR: Failure sending command message.");
        }
      }

      // prepare for sleep
      delay(500);
      break;
    }
    case Sleep:
    {
      // sleep is only used when
      // no peers connected
      // and search times out

      wakeup_check();
      break;
    }
    default: {
      // should never hit this, sanity check
      Serial.println("ERROR: Controller in unknown state!");
      break;
    }
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  // only raise issue if failure
  if (status == ESP_NOW_SEND_FAIL) {
    Serial.print("ERROR SENDING TO: ");
    Serial.println(*mac_addr);
  }
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incoming_data, int len)
{
  // handling method .. can move out of this and into separate recv_cb binds within the loop
  // this works for now (probably)
  Serial.println("RECEIVED");

  if (len != sizeof(ConnectMessage) && len != sizeof(MainMessage)) {
    // early return, bad data
    Serial.println("FAILED_RECV");
    return;
  }

  switch (state) {
    case TeamChoice: {
      ConnectMessage data;
      memcpy(&data, incoming_data, sizeof(data));

      if (data.command == LookingHost) {
        if (peers.total_num >= 2) {
          return;
        }

        Serial.println("RECEIVER MESSAGE RECVD");

        // add to peerlist
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, data.addr, 6);
        peer.channel = 0;
        peer.encrypt = false;
        
        if (esp_now_add_peer(&peer) == ESP_OK) {
          esp_now_get_peer_num(&peers);

          Serial.print("PEER CONNECTED: ");
          for (int i = 0; i < 6; i++) {
            Serial.print(data.addr[i]);
            if (i != 5) {
              Serial.print(":");
            }
          }
          Serial.println("");

          if (peers.total_num == 1) {
            last_heard[0] = esp_timer_get_time() / 1000;
          } else if (peers.total_num > 2) {
            Serial.println("ERROR: More than 2 peers connected!");
          } else {
            last_heard[1] = esp_timer_get_time() / 1000;
          }
        } else {
          // display error
          Serial.println("ERROR: Failure adding peer!");
        }
      } else {
        // debug message
        Serial.print("ERROR: Bad message received. State-found: ");
        Serial.print(data.command);
        Serial.print(". Delivered by: ");
        for (int i = 0; i < 6; i++) {
          Serial.print(data.addr[i]);
        }
        Serial.println("");
      }

      break;
    }
    case MainLoop: {
      ConnectMessage data;
      memcpy(&data, incoming_data, sizeof(data));

      // commands from
      switch (data.command)  {
        case Heartbeat: {
          
          break;
        }
        case Disconnect: {
          // attempt to remove peer
          // note: does this count wide addr? most other methods don't
          if (esp_now_del_peer(data.addr) == ESP_OK) {
            esp_now_get_peer_num(&peers);
            if (peers.total_num == 0) {
              state = Sleep;
            }
          } else {
            Serial.println("ERROR: Failure when removing peer!");
          }

          break;
        }
        default: {
          // sanity check
          break;
        }
      }
    }
    case Sleep: {
      // this should also check against cmd for a heartbeat
      break;
    }
    default: {
      break;
    }
  }
}

void pairing_call() {
  // build pairing message
  ConnectMessage data;
  get_mac(data.addr);
  data.command = LookingPeers;

  // send message widely
  esp_err_t res = esp_now_send(wide_addr, (uint8_t *)&data, sizeof(data));
  if (res != ESP_OK) {
    Serial.println("ERROR: Sending pairing message failed!");
  }

  return;
}

SweepCmds button_click() {
  // stop check, as its the most important command to come through
  // microseconds of difference, but still...
  Serial.print("BUTTON: ");
  Serial.print(digitalRead(BUTTON_ONE));
  Serial.print(" ");
  Serial.print(digitalRead(BUTTON_TWO));
  Serial.print(" ");
  Serial.print(digitalRead(BUTTON_THR));
  Serial.print(" ");
  Serial.print(digitalRead(BUTTON_FOU));
  Serial.print(" ");
  Serial.println(digitalRead(BUTTON_FIV));

  if (digitalRead(BUTTON_FIV)) {
    Serial.println("SENDING: STOP");
    return Stop;
  } else if (digitalRead(BUTTON_FOU)) {
    Serial.println("SENDING: HARD");
    return Hard;
  } else if (digitalRead(BUTTON_ONE)) {
    Serial.println("SENDING: CLEAN");
    return Clean;
  } else if (digitalRead(BUTTON_THR)) {
    Serial.println("SENDING: LEFT");
    return Left;
  } else if (digitalRead(BUTTON_TWO))  {
    Serial.println("SENDING: RIGHT");
    return Right;
  } else {
    // early return no pointer
    return None;
  }
}

void wakeup_check() {
  // i got a 'guru medfitation error' and it seems to be related to delay usage + interrupts
  // we are going to be doing something very disgusting
  if (digitalRead(BUTTON_ONE) || digitalRead(BUTTON_TWO) || digitalRead(BUTTON_THR) || digitalRead(BUTTON_FOU) || digitalRead(BUTTON_FIV)) {
    Serial.println("WAKEUP");
    state = TeamChoice;
  }
}
