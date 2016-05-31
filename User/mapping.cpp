#include "mapping.hpp"

// default mapping: Bayan (B-griff)
static_assert(SCAN_N_ROW == 10, "current mapping assumes 10 rows");
static_assert(SCAN_N_COL == 10, "current mapping assumes 10 cols");
uint8_t mapping[SCAN_N_ROW][SCAN_N_ROW] = {
    {34, 40, 46, 52, 58, 64, 70, 76, 82, 88}, // A# E
    {37, 43, 49, 55, 61, 67, 73, 79, 85, 91}, // C# G
    {32, 38, 44, 50, 56, 62, 68, 74, 80, 86}, // G# D
    {35, 41, 47, 53, 59, 65, 71, 77, 83, 89}, // B  F
    {33, 39, 45, 51, 57, 63, 69, 75, 81, 87}, // A  D#
    {36, 42, 48, 54, 60, 66, 72, 78, 84, 90}, // C  F#
    {31, 37, 43, 49, 55, 61, 67, 73, 79, 85}, // G  C#
    {34, 40, 46, 52, 58, 64, 70, 76, 82, 88}, // A# E
    {32, 38, 44, 50, 56, 62, 68, 74, 80, 86}, // G# D
    {35, 41, 47, 53, 59, 65, 71, 77, 83, 89}, // B  F
};
