#pragma once

#include <cstdint>

#ifdef DEBOUNCER_CHECK_TYPE
#   include <type_traits>
#endif // DEBOUNCER_CHECK_TYPE

template <typename T, T thres_transient, T thres_steady>
struct Debouncer {

#ifdef DEBOUNCER_CHECK_TYPE
    static_assert(std::is_signed<T>::value, "");
#endif // DEBOUNCER_CHECK_TYPE
    static_assert(thres_transient > 0, "");
    static_assert(thres_steady > thres_transient, "");

    static const T thres_transient_abs = thres_steady - thres_transient;
    static const bool LO = false, HI = true;

    // discrete integrator
    T counter;

    // finite state machine
    // | # | state              | out |
    // |---|--------------------|-----|
    // | 0 | seady-state lo     | lo  |
    // | 1 | transient lo-hi    | hi  |
    // | 2 | steady-state hi    | hi  |
    // | 3 | transient hi-lo    | lo  |
    std::uint8_t state;

    // always initialize from steady state
    void init(bool value) {
        if (value) {
            counter = +thres_steady;
            state = 2;
        } else {
            counter = -thres_steady;
            state = 0;
        }
    }
    Debouncer(bool value = LO) { init(value); }

    // shorthands
    operator bool () { return output(); }
    bool operator() (bool input) { return update(input); }

    // output is determined by state
    bool output() { return state == 1 || state == 2; }

    // run debouncing algorithm for one timestep
    // return: whether output has changed
    bool update(bool input) {
        if (input) {
            if (counter < +thres_steady) ++counter;
        } else {
            if (counter > -thres_steady) --counter;
        }
        switch (state) {
        case 0: // steady-state lo
            if (counter >= -thres_transient_abs) {
                // => transient lo-hi
                counter = 0;
                state = 1;
                return true;
            } else {
                return false;
            }
        case 1: // transient lo-hi
            switch (counter) {
            case +thres_steady:
                // => steady-state hi
                state = 2;
                return false;
            case -thres_steady:
                // => steady-state lo
                state = 0;
                return true;
            default:
                return false;
            }
        case 2: // steady-state hi
            if (counter <= +thres_transient_abs) {
                // => transient hi-lo
                counter = 0;
                state = 3;
                return true;
            } else {
                return false;
            }
        case 3: // transient hi-lo
            switch (counter) {
            case +thres_steady:
                // => steady-state hi
                state = 2;
                return true;
            case -thres_steady:
                // => steady-state lo
                state = 0;
                return false;
            default:
                return false;
            }
        default:
            // should panic, but `throw` isn't always available...
            return false;
        }
    }
};
