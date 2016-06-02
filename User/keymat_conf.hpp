#pragma once

#include <stdint.h>


////////////////////////////////////////
// hardware resources (peripherals) to use

// GPIO
// NOTE: all row pins must be in the same port (max 16 pins); col pins ditto
static const uint8_t SCAN_N_ROW = 10;
static const uint8_t SCAN_N_COL = 10;
#define SCAN_ROW_GPIO GPIOC
#define SCAN_COL_GPIO GPIOA
static const uint32_t SCAN_ROW_PINS[SCAN_N_ROW] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
static const uint32_t SCAN_COL_PINS[SCAN_N_COL] = {1, 4, 5, 6, 7, 8, 9, 10, 11, 12};

// TIM and associated DMA
// NOTE: TIM must have update and output compare DMA request lines mapped to
// different channels (e.g. TIM1 on STM32F0/1/3)
#define SCAN_TIM TIM1
#define SCAN_HDMA_UP hdma_tim1_up
#define SCAN_HDMA_CC hdma_tim1_ch4_trig_com


////////////////////////////////////////
// timing

// raw scanning
static const uint16_t SCAN_ROW_PERIOD_Tus = 20; // duration of a row being active within a scan cycle
static const uint16_t SCAN_READ_DELAY_Tus = 16; // time from writing a row to reading columns in that row
// derived
static const uint16_t SCAN_FIELD_PERIOD_Tus = SCAN_ROW_PERIOD_Tus * SCAN_N_ROW; // total time to complete a scan cycle

// debouncing
static const uint32_t SCAN_BOUNCE_THRES_STEADY_Tus = 6000;
static const uint32_t SCAN_BOUNCE_THRES_TRANSIENT_Tus = 600;
typedef int8_t scan_debounce_counter_t;
// derived
#define CEIL_DIV(a, b) (((a) + (b) - 1) / (b))
static const uint32_t SCAN_BOUNCE_THRES_STEADY_n = CEIL_DIV(SCAN_BOUNCE_THRES_STEADY_Tus, SCAN_FIELD_PERIOD_Tus);
static const uint32_t SCAN_BOUNCE_THRES_TRANSIENT_n = CEIL_DIV(SCAN_BOUNCE_THRES_TRANSIENT_Tus, SCAN_FIELD_PERIOD_Tus);
#undef CEIL_DIV
