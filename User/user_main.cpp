#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "cmsis_os.h"

#include <string.h>

#include "keymat.hpp"


////////////////////////////////////////
// UART (simple helpers)

#define HUART huart1

static const size_t N_BUF = 64;
static char buf[N_BUF];
static void send_1(unsigned char c) {
    HAL_UART_Transmit(&HUART, &c, 1, HAL_MAX_DELAY);
}
static void send_len() {
    HAL_UART_Transmit(&HUART, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
}
static void send_n(size_t n) {
    HAL_UART_Transmit(&HUART, (uint8_t*)buf, n, HAL_MAX_DELAY);
}


////////////////////////////////////////
// key event handling

// default (hardcoded) mapping: Bayan (B-griff)
static_assert(KEYMAT_ROW_n == 10, "current mapping assumes 10 rows");
static_assert(KEYMAT_COL_n == 10, "current mapping assumes 10 cols");
typedef uint8_t keycode_t;
keycode_t mapping[KEYMAT_ROW_n][KEYMAT_COL_n] = {
    {34, 40, 46, 52, 58, 64, 70, 76, 82, 88}, // A# E
    {37, 43, 49, 55, 61, 67, 73, 79, 85, 91}, // C# G
    {35, 41, 47, 53, 59, 65, 71, 77, 83, 89}, // B  F
    {38, 44, 50, 56, 62, 68, 74, 80, 86, 92}, // D  G#
    {33, 39, 45, 51, 57, 63, 69, 75, 81, 87}, // A  D#
    {36, 42, 48, 54, 60, 66, 72, 78, 84, 90}, // C  F#
    {34, 40, 46, 52, 58, 64, 70, 76, 82, 88}, // A# E
    {37, 43, 49, 55, 61, 67, 73, 79, 85, 91}, // C# G
    {32, 38, 44, 50, 56, 62, 68, 74, 80, 86}, // G# D
    {35, 41, 47, 53, 59, 65, 71, 77, 83, 89}, // B  F
};

struct KeyEvent {
    keycode_t keycode;
    bool state;
};
osMailQDef(key_events, 8, KeyEvent);
osMailQId key_events;

static void key_event_init() {
    key_events = osMailCreate(osMailQ(key_events), NULL);
}

// NOTE: callback from ISR -- cannot wait
static void key_event_handler(uint8_t ri, uint8_t ci, bool state) {
    KeyEvent* e = (KeyEvent*)osMailAlloc(key_events, 0);
    if (!e) return;
    e->keycode = mapping[ri][ci];
    e->state = state;
    osMailPut(key_events, e);
}


////////////////////////////////////////
// main thread

extern "C" void user_main() {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

    osKernelInitialize();
    osKernelStart();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);

    key_event_init();
    keymat_init();
    keymat_callback = key_event_handler;
    keymat_start();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

    while (1) {
        osEvent ose = osMailGet(key_events, osWaitForever);
        if (ose.status == osEventMail) {
            KeyEvent* e = (KeyEvent*)ose.value.p;

            // NOTE: MIDI handling is hardcoded for now
            buf[0] = (e->state ? 0x90 : 0x80); // use ch0
            buf[1] = e->keycode;
            buf[2] = 100; // use hard-coded velocity

            osMailFree(key_events, e);

            send_n(3); // blocking call
        }
    }
}
