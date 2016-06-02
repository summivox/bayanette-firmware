#include "keymat.hpp"

#include <string.h>

#include "dma.h"
#include "tim.h"

#include "bitband.h"
#include "debouncer.hpp"


////////////////////////////////////////
// hardware-driven keyboard matrix scanning

// keymat_out: const table; entries are DMA'd to GPIO port to generate 1-hot
// row scanning signal
//
// NOTE: not acutally const in order to:
// - keep it in SRAM instead of FLASH
// - avoid template metaprogramming yet still specify pins in conf directly
static uint32_t keymat_out[KEYMAT_ROW_n];
static uint32_t keymat_out_clear; // for clearing all row outputs

// keymat_in: double buffer; stores raw input from DMA (GPIO pin state snapshots)
// NOTE: 32-bit for DMA transfer; ordered by GPIO pin#, not col#
static volatile uint32_t keymat_in[2][KEYMAT_ROW_n];

// populate keymat_out
static void keymat_out_init() {
    // BSRR[31:16]: a `1` sets corresponding pin to low
    uint32_t all = 0;
    for (size_t i = 0 ; i < KEYMAT_ROW_n ; ++i) {
        all |= 1 << KEYMAT_ROW_PINS[i];
    }
    all <<= 16;
    keymat_out_clear = all;

    // BSRR[15:0]: a `1` sets corresponding pin to high
    // NOTE: has priority over BSRR[31:16] -- no need to mask bit off `all`
    for (size_t i = 0 ; i < KEYMAT_ROW_n ; ++i) {
        keymat_out[i] = (1 << KEYMAT_ROW_PINS[i]) | all;
    }
}


////////////////////////////////////////
// debouncing

#define CEIL_DIV(a, b) (((a) + (b) - 1) / (b))
static Debouncer<
    keymat_debounce_counter_t,
    CEIL_DIV(KEYMAT_BOUNCE_THRES_TRANSIENT_Tus, KEYMAT_FIELD_PERIOD_Tus),
    CEIL_DIV(KEYMAT_BOUNCE_THRES_STEADY_Tus, KEYMAT_FIELD_PERIOD_Tus)
    > debouncer[KEYMAT_ROW_n][KEYMAT_COL_n];

// run debouncing algorithm when a full snapshot has been captured
// half: which half of the double buffer `keymat_in` contains the most recent snapshot
static void keymat_debounce_field(uint8_t half) {
    volatile uint32_t* in = keymat_in[half];
    for (size_t ri = 0 ; ri < KEYMAT_ROW_n ; ++ri) {
        uint32_t row = in[ri];
        for (size_t ci = 0 ; ci < KEYMAT_COL_n ; ++ci) {
            bool input = (row >> KEYMAT_COL_PINS[ci]) & 1;
            bool changed = debouncer[ri][ci].update(input);
            if (changed) {
                bool output = debouncer[ri][ci].output();
                // atomically write state bit using Cortex-M bit-band alias
                SBIT_RAM(keymat_state + ri, ci) = output;
                // callback might not be registered
                if (keymat_callback) keymat_callback(ri, ci, output);
            }
        }
    }
}


////////////////////////////////////////
// peripheral interface

// DMA handles not directly exposed by HAL
extern DMA_HandleTypeDef KEYMAT_HDMA_UP;
extern DMA_HandleTypeDef KEYMAT_HDMA_CC;

// DMA interrupt callbacks: snapshot captured; run debouncing
static void keymat_half_cb(DMA_HandleTypeDef* hdma) { keymat_debounce_field(0); }
static void keymat_full_cb(DMA_HandleTypeDef* hdma) { keymat_debounce_field(1); }

// forward decl
static void keymat_hw_init();
static void keymat_hw_start();
static void keymat_hw_stop();

// setup peripherals
static void keymat_hw_init() {
    // setup DMA using HAL
    KEYMAT_HDMA_CC.XferHalfCpltCallback = keymat_half_cb;
    KEYMAT_HDMA_CC.XferCpltCallback = keymat_full_cb;

    // setup TIM directly with registers (easier than using HAL)
    KEYMAT_TIM->PSC = SystemCoreClock/1000000 - 1; // 1us tick (assuming timer clock freq same as CPU)
    KEYMAT_TIM->ARR = KEYMAT_ROW_PERIOD_Tus - 1;
    KEYMAT_TIM->CCR4 = KEYMAT_READ_DELAY_Tus;
    KEYMAT_TIM->CCER = TIM_CCER_CC4E; // enable output compare
    KEYMAT_TIM->DIER = TIM_DIER_UDE | TIM_DIER_CC4DE; // enable DMA requests
}

// start scanning
static void keymat_hw_start() {
    // reset if already started
    keymat_hw_stop();
    // start DMA
    HAL_DMA_Start   (&KEYMAT_HDMA_UP, (uint32_t)keymat_out, (uint32_t)&(KEYMAT_ROW_GPIO->BSRR), KEYMAT_ROW_n);
    HAL_DMA_Start_IT(&KEYMAT_HDMA_CC, (uint32_t)&(KEYMAT_COL_GPIO->IDR),   (uint32_t)keymat_in, KEYMAT_ROW_n*2);
    // start timer
    KEYMAT_TIM->EGR = TIM_EGR_UG; // reset counter to 0 and generate initial output DMA transfer
    KEYMAT_TIM->CR1 |= TIM_CR1_CEN;
}

// stop scanning
static void keymat_hw_stop() {
    // stop timer
    KEYMAT_TIM->CR1 &=~ TIM_CR1_CEN;
    // stop DMA
    HAL_DMA_Abort(&KEYMAT_HDMA_UP);
    HAL_DMA_Abort(&KEYMAT_HDMA_CC);
    // EXTRA: clear GPIO
    KEYMAT_ROW_GPIO->BSRR = keymat_out_clear;
}


////////////////////////////////////////
// public interface

volatile uint16_t keymat_state[KEYMAT_ROW_n];

extern keymat_callback_t keymat_callback = nullptr;

void keymat_init() {
    keymat_out_init();
    keymat_hw_init();
}
void keymat_start() { keymat_hw_start(); }
void keymat_stop() { keymat_hw_stop(); }
