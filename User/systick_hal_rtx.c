int os_tick_init (void) { return (-1); }
unsigned HAL_GetTick(void) {
    extern unsigned os_time;
    return os_time;
}
