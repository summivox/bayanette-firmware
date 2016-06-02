#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "cmsis_os.h"

#include <string.h>

#include "scan.hpp"
#include "mapping.hpp"


////////////////////////////////////////
// UART

#define HUART huart1

static const size_t N_BUF = 100;
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
// key event queue

struct KeyEvent {
    uint8_t ri, ci, state;
};
osMailQDef(key_events, 8, KeyEvent);
osMailQId key_events;

static void key_event_init() {
    key_events = osMailCreate(osMailQ(key_events), NULL);
}

// NOTE: callback from ISR -- cannot wait
static void key_event_handler(uint8_t ri_, uint8_t ci_, uint8_t state_) {
    KeyEvent* e = (KeyEvent*)osMailAlloc(key_events, 0);
    if (!e) return;
    e->ri = ri_;
    e->ci = ci_;
    e->state = state_;
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
    scan_init();
    scan_callback = key_event_handler;
    scan_start();
    
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

    while (1) {
        osEvent ose = osMailGet(key_events, osWaitForever);
        if (ose.status == osEventMail) {
            KeyEvent* e = (KeyEvent*)ose.value.p;
            
            // NOTE: MIDI handling is hardcoded for now
            buf[0] = (e->state ? 0x90 : 0x80); // use ch0
            buf[1] = mapping[e->ri][e->ci];
            buf[2] = 100; // use hard-coded velocity
            
            osMailFree(key_events, e);
            
            send_n(3); // blocking call
        }
    }
}
