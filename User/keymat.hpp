#pragma once
#include "keymat_conf.hpp"

#include <stdint.h>

// keymat_state: array of bit vectors; each bit is the current sampled state of a key
// row#: array index
// col#: bit index
extern volatile uint16_t keymat_state[KEYMAT_ROW_n];

// convenience function for checking the status of a single key
static bool keymat_state_get(uint8_t ri, uint8_t ci) {
    return (keymat_state[ri] >> ci) & 1;
}

// event callback: notify that a key has changed state
// NOTE: called indirectly from ISR
typedef void (*keymat_callback_t)(uint8_t ri, uint8_t ci, bool state);
extern keymat_callback_t keymat_callback;

// actions
void keymat_init(void);
void keymat_start(void);
void keymat_stop(void);
