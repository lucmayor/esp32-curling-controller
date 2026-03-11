#include <Arduino.h>

// Controller state enum.
enum Stage
{
    TeamChoice,
    MainLoop,
    Sleep
};

// pairing system payload.
typedef struct payload
{
    uint8_t command;
    uint8_t addr[6];
} Message;

typedef struct cache
{
    Message msg;
    int64_t timestamp;
} CachedMessage;