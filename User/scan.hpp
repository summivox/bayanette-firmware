#pragma once
#include "scan_conf.hpp"

#include <stdint.h>

// scan_state: array of bit vectors; each bit is the current sampled state of a key
// row#: array index
// col#: bit index
extern volatile uint16_t scan_state[SCAN_N_ROW];

// event callback: notify that a key has changed state
typedef void (*scan_callback_t)(uint8_t ri, uint8_t ci, uint8_t state);
extern scan_callback_t scan_callback;

void scan_init(void);
void scan_start(void);
