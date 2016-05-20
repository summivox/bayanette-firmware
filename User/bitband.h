#pragma once

#include <stdint.h>

// Cortex-M Bitband alias macros
// PBIT(addr, bit) returns the address to the bitband alias
// SBIT(addr, bit) returns reference to the bitband alias
#define PBIT(addr, bit) (((uint32_t)(addr)&0xF0000000)+0x02000000+(((((uint32_t)(addr)&0xFFFFF)<<3)+(bit))<<2))
#define SBIT(addr, bit) (*(volatile uint32_t*)(PBIT(addr, bit)))
#define PBIT_RAM(addr, bit) ( 0x22000000+(((((uint32_t)(addr)&0xFFFFF)<<3)+(bit))<<2))
#define SBIT_RAM(addr, bit) (*(volatile uint32_t*)(PBIT_RAM(addr, bit)))
