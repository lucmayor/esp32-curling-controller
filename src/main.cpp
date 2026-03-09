#include <Arduino.h>

// put function declarations here:
enum Stage
{
  TeamChoice,
  MainLoop,
  Sleep
};

typedef struct payload
{
  uint8_t command;
} Message;

typedef struct cache {
  Message msg;
  int64_t timestamp;
} CachedMessage;

// global cmds
Stage state;

void setup()
{
  state = TeamChoice;
}

void loop()
{
  switch (state) {
    case TeamChoice:
      break;
    case MainLoop:
      break;
    case Sleep:
      break;
    default:
      break;
  }
}

// put function definitions here: