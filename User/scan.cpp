#include "scan.hpp"

#include <string.h>
#include <math.h>

#include "dma.h"
#include "tim.h"

#include "bitband.h"
#include "debouncer.hpp"


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


////////////////////////////////////////
// debouncing

static Debouncer<
    scan_debounce_counter_t,
    SCAN_BOUNCE_THRES_TRANSIENT_n,
    SCAN_BOUNCE_THRES_STEADY_n> debouncer[SCAN_N_ROW][SCAN_N_COL];

// run debouncing algorithm using last full-field snapshot in `scan_in_raw`
// half: which half of double buffer to process
static void scan_debounce_field(uint8_t half) {
    volatile uint32_t* scan_in = scan_in_raw[half];
    for (size_t ri = 0 ; ri < SCAN_N_ROW ; ++ri) {
        uint32_t raw_row = scan_in[ri];
        for (size_t ci = 0 ; ci < SCAN_N_COL ; ++ci) {
            bool input = (raw_row >> SCAN_COL_PINS[ci]) & 1;
            bool changed = debouncer[ri][ci].update(input);
            if (changed) {
                bool output = debouncer[ri][ci].output();
                // atomically write state bit using Cortex-M bit-band alias
                SBIT_RAM(scan_state + ri, ci) = output;
                // callback might not be registered
                if (scan_callback) scan_callback(ri, ci, output);
            }
        }
    }
}


////////////////////////////////////////
// interface with HAL

// interrupt callback
static void scan_half_cb(DMA_HandleTypeDef* hdma) { scan_debounce_field(0); }
static void scan_full_cb(DMA_HandleTypeDef* hdma) { scan_debounce_field(1); }


////////////////////////////////////////
// public interface

volatile uint16_t scan_state[SCAN_N_ROW];

extern scan_callback_t scan_callback = nullptr;

void scan_init() {
    scan_out_init();
}

// setup scanning capture
void scan_start() {
    // DMA handles not directly exposed by HAL
    extern DMA_HandleTypeDef SCAN_HDMA_UP;
    extern DMA_HandleTypeDef SCAN_HDMA_CC;

    // setup DMA using STM32 HAL API
    SCAN_HDMA_CC.XferHalfCpltCallback = scan_half_cb;
    SCAN_HDMA_CC.XferCpltCallback = scan_full_cb;
    HAL_DMA_Start   (&SCAN_HDMA_UP, (uint32_t)scan_out_raw, (uint32_t)&(SCAN_ROW_GPIO->BSRR), SCAN_N_ROW);
    HAL_DMA_Start_IT(&SCAN_HDMA_CC, (uint32_t)&(SCAN_COL_GPIO->IDR),   (uint32_t)scan_in_raw, SCAN_N_ROW*2);

    // setup TIM directly with registers (easier than HAL actually)
    SCAN_TIM->PSC = SystemCoreClock/1e6 - 1; // 1us tick (assuming timer clock freq same as CPU)
    SCAN_TIM->ARR = SCAN_ROW_PERIOD_Tus - 1;
    SCAN_TIM->CCR4 = SCAN_READ_DELAY_Tus;
    SCAN_TIM->CCER = TIM_CCER_CC4E; // enable output compare
    SCAN_TIM->DIER = TIM_DIER_UDE | TIM_DIER_CC4DE; // enable DMA requests
    SCAN_TIM->EGR = TIM_EGR_UG; // reset timer -- this generates initial output DMA transfer
    SCAN_TIM->CR1 |= TIM_CR1_CEN; // start timer
}
