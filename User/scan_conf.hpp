#pragma once

#include <stdint.h>
#include <limits>


////////////////////////////////////////
// hardware mapping

// GPIO
// NOTE: all row pins must be in the same port (max 16 pins); col pins ditto
static const uint8_t SCAN_N_ROW = 10;
static const uint8_t SCAN_N_COL = 10;
#define SCAN_ROW_GPIO GPIOB
#define SCAN_COL_GPIO GPIOA
static const uint32_t SCAN_ROW_PINS[SCAN_N_ROW] = {0, 1, 2, 10, 11, 12, 3, 7, 8, 9};
static const uint32_t SCAN_COL_PINS[SCAN_N_COL] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 15};

// TIM and associated DMA
// NOTE: TIM must have update and output compare DMA request lines mapped to
// different channels (e.g. TIM1 on STM32F10x)
#define SCAN_TIM TIM1
#define SCAN_HDMA_UP hdma_tim1_ch3_up
#define SCAN_HDMA_CC hdma_tim1_ch4_trig_com


////////////////////////////////////////
// timing

// raw scanning
static const uint16_t SCAN_ROW_PERIOD_Tus = 20; // duration of a row being active within a scan cycle
static const uint16_t SCAN_READ_DELAY_Tus = 16; // time from writing a row to reading columns in that row
// derived
static const uint16_t SCAN_FIELD_PERIOD_Tus = SCAN_ROW_PERIOD_Tus * SCAN_N_ROW; // total time to complete a scan cycle

// debouncing
// NOTE:
// - times specified here are effectively rounded to integer multiples of `SCAN_FIELD_PERIOD_Tus`
// - relationship between times: lo < hi < max
// - debouncing counter type must be unsigned and able to store `SCAN_BOUNCE_MAX_n`
static const uint32_t SCAN_BOUNCE_MAX_Tus = 6000;
static const uint32_t SCAN_BOUNCE_THRES_HI_Tus = 800;
static const uint32_t SCAN_BOUNCE_THRES_LO_Tus = 400;
typedef uint8_t scan_counter_t;
// derived
static const uint32_t SCAN_BOUNCE_MAX_n = SCAN_BOUNCE_MAX_Tus/SCAN_FIELD_PERIOD_Tus;
static const uint32_t SCAN_BOUNCE_THRES_HI_n = SCAN_BOUNCE_THRES_HI_Tus/SCAN_FIELD_PERIOD_Tus;
static const uint32_t SCAN_BOUNCE_THRES_LO_n = SCAN_BOUNCE_THRES_LO_Tus/SCAN_FIELD_PERIOD_Tus;
static_assert(SCAN_BOUNCE_THRES_LO_n < SCAN_BOUNCE_THRES_HI_n, "lo < hi");
static_assert(SCAN_BOUNCE_THRES_HI_n < SCAN_BOUNCE_MAX_n, "hi < max");
static const scan_counter_t scan_counter_max = ~(scan_counter_t)0;
static_assert(SCAN_BOUNCE_MAX_n <= scan_counter_max, "max <= counter type max");
