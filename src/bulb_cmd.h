#pragma once
#define BULB_COUNT 8
#include <stdint.h>

/* One "set this bulb to this colour" request */
struct bulb_cmd {
    uint8_t bulb_id;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
