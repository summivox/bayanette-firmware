#pragma once

#include <stdint.h>


////////////////////////////////////////
// keyboard matrix dimensions

static const uint8_t KEYMAT_ROW_n = 10;
static const uint8_t KEYMAT_COL_n = 10;
// check
static_assert(1 <= KEYMAT_ROW_n && KEYMAT_ROW_n <= 16, "");
static_assert(1 <= KEYMAT_COL_n && KEYMAT_COL_n <= 16, "");


////////////////////////////////////////
// hardware resources (peripherals)

// GPIO
// NOTE: all row pins must be in the same port (max 16 pins); col pins ditto
#define KEYMAT_ROW_GPIO GPIOA
#define KEYMAT_COL_GPIO GPIOB
static const uint8_t KEYMAT_ROW_PINS[KEYMAT_ROW_n] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 15};
static const uint8_t KEYMAT_COL_PINS[KEYMAT_COL_n] = {0, 1, 2, 10, 11, 12, 15, 7, 8, 9}; // FIXME: col[6] should be 3; temporary work around bad IO pin

// TIM and associated DMA
// NOTE: TIM must have update and output compare DMA request lines mapped to
// different channels (e.g. TIM1 on STM32F0/1/3)
#define KEYMAT_TIM TIM1
#define KEYMAT_HDMA_UP hdma_tim1_ch3_up
#define KEYMAT_HDMA_CC hdma_tim1_ch4_trig_com


////////////////////////////////////////
// timing

// raw scanning
static const uint16_t KEYMAT_ROW_PERIOD_Tus = 30; // duration of a row being active within a scan cycle
static const uint16_t KEYMAT_READ_DELAY_Tus = 20; // time from writing a row to reading columns in that row
// derived
static const uint16_t KEYMAT_FIELD_PERIOD_Tus = KEYMAT_ROW_PERIOD_Tus * KEYMAT_ROW_n; // total time to complete a scan cycle

// debouncing
static const uint32_t KEYMAT_BOUNCE_THRES_STEADY_Tus = 6000;
static const uint32_t KEYMAT_BOUNCE_THRES_TRANSIENT_Tus = 600;
typedef int8_t keymat_debounce_counter_t;
