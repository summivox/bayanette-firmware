#include "scan.hpp"

#include <string.h>
#include <math.h>

#include "dma.h"
#include "tim.h"

#include "bitband.h"

// DMA handles not directly exposed by HAL
extern DMA_HandleTypeDef SCAN_HDMA_UP;
extern DMA_HandleTypeDef SCAN_HDMA_CC;


////////////////////////////////////////
// raw scanning

// scan_out_raw: const table; entries are DMA'd to GPIO port to generate 1-hot
// row scanning signal
//
// NOTE: not acutally const in order to:
// - keep it in SRAM instead of FLASH
// - avoid template metaprogramming yet still specify pins in conf directly
static uint32_t scan_out_raw[SCAN_N_ROW];

// scan_in_raw: double buffer; stores raw input from DMA (GPIO pin state snapshots)
// NOTE: 32-bit for DMA transfer; ordered by GPIO pin#, not col#
static volatile uint32_t scan_in_raw[2][SCAN_N_ROW];

// scan_in: same format as `scan_state`; not debounced
static uint16_t scan_in[SCAN_N_ROW];

// populate scan_out_raw
static void scan_out_init() {
    // BSRR[31:16]: a `1` sets corresponding pin to low
    uint32_t all = 0;
    for (size_t i = 0 ; i < SCAN_N_ROW ; ++i) {
        all |= 1 << SCAN_ROW_PINS[i];
    }
    all <<= 16;
    // BSRR[15:0]: a `1` sets corresponding pin to high
    // NOTE: has priority over BSRR[31:16] -- no need to mask bit off `all`
    for (size_t i = 0 ; i < SCAN_N_ROW ; ++i) {
        scan_out_raw[i] = (1 << SCAN_ROW_PINS[i]) | all;
    }
}

// extract last full-field snapshot in `scan_in_raw` to `scan_in`
// half: which half of double buffer to copy from
static void scan_dump(uint8_t half) {
    volatile uint32_t* a = scan_in_raw[half];
    for (size_t ri = 0 ; ri < SCAN_N_ROW ; ++ri) {
        uint32_t raw_row = a[ri];
        uint16_t row = 0;
        for (size_t ci = 0 ; ci < SCAN_N_COL ; ++ci) {
            row |= ((raw_row >> SCAN_COL_PINS[ci]) & 1) << ci;
        }
        scan_in[ri] = row;
    }
}


////////////////////////////////////////
// debouncing

// discrete integrator for each key
static scan_counter_t counter[SCAN_N_ROW][SCAN_N_COL];

// finite state machine for each key
// | # | state              | out |
// |---|--------------------|-----|
// | 0 | stable lo          | lo  |
// | 1 | lo -> hi transient | hi  |
// | 2 | stable hi          | hi  |
// | 3 | hi -> lo transient | lo  |
//
// NOTE: not to be confused with `scan_state`
static uint8_t state[SCAN_N_ROW][SCAN_N_COL];

static void scan_debounce_init() {
    memset(counter, 0, sizeof(counter));
    memset(state, 0, sizeof(state));
}

static inline void output(uint8_t ri, uint8_t ci, uint8_t state) {
    SBIT_RAM(scan_state + ri, ci) = state; // change bit using Cortex-M bit-band alias
    if (scan_callback) scan_callback(ri, ci, state);
}

static void scan_debounce_field(uint8_t half) {
    scan_dump(half);
    for (size_t ri = 0 ; ri < SCAN_N_ROW ; ++ri) {
        uint16_t row = scan_in[ri];
        // NOTE: symmetric increment for simple impl
        for (size_t ci = 0 ; ci < SCAN_N_COL ; ++ci) {
            scan_counter_t c = counter[ri][ci];
            if ((row >> ci) & 1) {
                // input is high => increment counter
                if (c < SCAN_BOUNCE_MAX_n) ++c;
                else continue;
            } else {
                // input is low  => decrement counter
                if (c > 0) --c;
                else continue;
            }
            counter[ri][ci] = c;

            switch (state[ri][ci]) {
            case 0: // stable low
                if (c >= SCAN_BOUNCE_THRES_HI_n) {
                    state[ri][ci] = 1;
                    output(ri, ci, 1);
                }
                break;
            case 1: // low -> high transient
                if (c < SCAN_BOUNCE_THRES_LO_n) {
                    state[ri][ci] = 0;
                    output(ri, ci, 0);
                }
                if (c == SCAN_BOUNCE_MAX_n) {
                    state[ri][ci] = 2;
                }
                break;
            case 2: // stable high
                if (c <= SCAN_BOUNCE_MAX_n - SCAN_BOUNCE_THRES_HI_n) {
                    state[ri][ci] = 3;
                    output(ri, ci, 0);
                }
                break;
            case 3: // high -> low transient
                if (c > SCAN_BOUNCE_MAX_n - SCAN_BOUNCE_THRES_LO_n) {
                    state[ri][ci] = 2;
                    output(ri, ci, 1);
                }
                if (c == 0) {
                    state[ri][ci] = 0;
                }
                break;
            default:
                // TODO: panic!
                break;
            }
        }
    }
}

// HAL interrupt callback
static void scan_half_cb(DMA_HandleTypeDef* hdma) { scan_debounce_field(0); }
static void scan_full_cb(DMA_HandleTypeDef* hdma) { scan_debounce_field(1); }


////////////////////////////////////////
// public interface

volatile uint16_t scan_state[SCAN_N_ROW];

extern scan_callback_t scan_callback = nullptr;

void scan_init() {
    scan_out_init();
    scan_debounce_init();
}

void scan_start() {
    SCAN_HDMA_CC.XferHalfCpltCallback = scan_half_cb;
    SCAN_HDMA_CC.XferCpltCallback = scan_full_cb;
    HAL_DMA_Start   (&SCAN_HDMA_UP, (uint32_t)scan_out_raw, (uint32_t)&(SCAN_ROW_GPIO->BSRR), SCAN_N_ROW);
    HAL_DMA_Start_IT(&SCAN_HDMA_CC, (uint32_t)&(SCAN_COL_GPIO->IDR),   (uint32_t)scan_in_raw, SCAN_N_ROW*2);

    SCAN_TIM->PSC = SystemCoreClock/1e6 - 1; // 1us tick
    SCAN_TIM->ARR = SCAN_ROW_PERIOD_Tus - 1;
    SCAN_TIM->CCR4 = SCAN_READ_DELAY_Tus;
    SCAN_TIM->CCER = TIM_CCER_CC4E; // enable output compare
    SCAN_TIM->DIER = TIM_DIER_UDE | TIM_DIER_CC4DE; // enable DMA requests
    SCAN_TIM->EGR = TIM_EGR_UG; // reset timer -- this generates initial output DMA transfer
    SCAN_TIM->CR1 |= TIM_CR1_CEN; // start timer
}
