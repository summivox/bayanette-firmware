#include "stm32f1xx_hal.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include <string.h>

#include "scan.hpp"

////////////////////////////////////////
// Main

static const size_t N_BUF = 100;
static char buf[N_BUF];
static void send_1(unsigned char c) {
    HAL_UART_Transmit(&huart3, &c, 1, HAL_MAX_DELAY);
}
static void send_n() {
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
}

bool notified = false;
uint8_t ri, ci, state;
static void notify(uint8_t ri_, uint8_t ci_, uint8_t state_) {
    notified = true;
    ri = ri_; ci = ci_; state = state_;
}

extern "C" void user_main() {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);
    strncpy(buf, "\r\n" "bayan-test" "\r\n", N_BUF);
    send_n();
    
    scan_init();
    scan_callback = notify;
    scan_start();

#if 0
    // transfer data
    while (1) {
        HAL_Delay(250);
        for (size_t ri = 0 ; ri < SCAN_N_ROW ; ++ri) {
            uint16_t row = scan_state[ri];
            for (size_t ci = 0 ; ci < SCAN_N_COL ; ++ci) {
                send_1(((row >> ci) & 1) ? '1' : '0');
            }
            strncpy(buf, "\r\n", N_BUF);
            send_n();
        }
        strncpy(buf, "\r\n" "----------" "\r\n", N_BUF);
        send_n();
    }
#endif
    size_t counter = 0;
    while (1) {
        if (notified) {
            notified = false;
            sprintf(buf, "%05d: %c%d%d\r\n", counter++, state ? '+' : '-', ri, ci); // assume single digit
            send_n();
        }
    }
}
