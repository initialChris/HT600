#include "HT600.h"

/**
 * @brief Constructor for the HT680 decoder.
 * * Calculations based on HT680 Datasheet:
 * - One symbol clock (T) = 1 / (fosc / 33)
 * - "Short" pulse = 1T
 * - "Long"  pulse = 2T
 * - "Pilot" pulse = 36T (Transmission begins with a LOW pulse lasting this interval)
 * * @param fosc_khz The oscillation frequency based on Rosc (use HT680_XXXK_FOSC macros).
 * @param tolerance Percentage of error allowed (e.g., 30 for 30%) Avoid values greater than ~30 to avoid overlapping beetwen short and long pulses timing.
 * @param tick_length_us The resolution of the timestamp source in microseconds (e.g., 1 for micros()).
 * @param noise_filter_us Minimum duration between transitions to filter out noise (e.g., 50 for 50 microseconds).
 */
HT600::HT600(const uint16_t fosc_khz, const uint8_t tolerance, const uint16_t tick_length_us, const uint16_t noise_filter_us) {
    // T (period in microseconds) = 1000 / (fosc_khz / 33) = 33000 / fosc_khz
    float base_period_us = 33000.0 / fosc_khz;

    // Converting the base period in microseconds to the number of ticks
    float T_ticks = base_period_us / tick_length_us;

    // Defining pulse length constraints with tolerance:
    // Short pulse (1T): Used for '0' (H), '1' (L), 'Open' (Both)
    _short_tick_min = uint16_t(T_ticks * (100 - tolerance) /100);
    _short_tick_max = uint16_t(T_ticks * (100 + tolerance) /100);

    // Long pulse (2T): Used for '0' (L), '1' (H)
    _long_tick_min  = uint16_t((T_ticks * 2.0) * (100 - tolerance) /100);
    _long_tick_max  = uint16_t((T_ticks * 2.0) * (100 + tolerance) /100);

    // Pilot period (36T): Minimal LOW duration to identify a new transmission
    // Since the pilot period is 6 bits long and each bit takes up 6T, it lasts 36T
    _pilot_tick_min = uint16_t((T_ticks * 36.0) * (100 - tolerance) /100);

    // Noise filter threshold in ticks
    _noise_filter_tick = uint16_t(noise_filter_us / tick_length_us);

    this -> reset();
}

/**
 * @brief Event processor for the HT600 decoder logic.
 * * This function must be called by an external ISR dispatcher. It calculates 
 * the time delta between transitions to decode the trinary signal.
 * * @param pinState The current logical state of the input pin (true/false).
 * @param ticks The current timestamp in ticks (Resolution must match tick_length_us).
 */
void HT600::processEvent(const bool pinState, const uint32_t ticks) {
    // If the state is DONE, wait until the results are handled by the main loop
    if (_state == HT600_STATE::DONE) return;

    // Timing calculations
    uint32_t now = ticks;
    uint32_t delta = now - _last_interrupt_tick;
    
    // Ignore transitions that are too close together (de-glitch filter)
    if (delta < _noise_filter_tick) return; 

    _last_interrupt_tick = now;

    // If pinState is true (Rising Edge), store the duration of the preceding LOW period
    if (pinState == true) {
        _period_L = (delta > 0xFFFF) ? 0xFFFF : (uint16_t)delta;
        return; // Logic continues on the next Falling Edge
    }

    // If pinState is false (Falling Edge), store the duration of the preceding HIGH period
    _period_H = (delta > 0xFFFF) ? 0xFFFF : (uint16_t)delta;

    // IDLE State: Looking for the Pilot signal (long LOW pulse) followed by the first SYNC pulse
    if (_state == HT600_STATE::IDLE) {
        // A valid Pilot is a long LOW followed by a SHORT HIGH pulse
        if (_period_L > _pilot_tick_min && HT600_IS_IN_RANGE(_period_H, _short_tick_min, _short_tick_max)) {
            _state = HT600_STATE::READING; 
            _bit_index = 0;
            
            _half_symbol_read = false; 

        }
        // Nothing else to do in IDLE state, just wait for the next transition
        return;
    }

    // If current state is SYNC_1, SYNC_2 or READING, decode the symbols
    bool current_symbol = 0;
    if (HT600_IS_IN_RANGE(_period_L, _short_tick_min, _short_tick_max) && HT600_IS_IN_RANGE(_period_H, _long_tick_min, _long_tick_max)) {
        current_symbol = 0;
    }
    else if (HT600_IS_IN_RANGE(_period_L, _long_tick_min, _long_tick_max) && HT600_IS_IN_RANGE(_period_H, _short_tick_min, _short_tick_max)) {
        current_symbol = 1;
    }
    else if (_period_L > _pilot_tick_min && HT600_IS_IN_RANGE(_period_H, _short_tick_min, _short_tick_max)) {
        // This is a special case where we might have a new pilot signal in the middle of reading, maybe due to noise or a new transmission starting.
        // Set the state to SYNC_1 and wait for the next transition
        _state = HT600_STATE::READING;
        _bit_index = 0;
        _half_symbol_read = false;
        return;
    }
    else {
        // Bad timing, reset to IDLE and wait for the next transition
        _state = HT600_STATE::IDLE;
        return;
    }

    if (!_half_symbol_read) {
        // If we are reading the first half of the symbol, store the current symbol and wait for the next transition
        _half_symbol_read = true;
        _last_symbol = current_symbol;
        return;
    }
    
    // If we are reading the second half of the symbol, we can decode the bit
    _half_symbol_read = false;

    if (_state == HT600_STATE::READING) {
        // Bit 0 & 1: SYNC Pattern validation (Must be SYMBOL1 + SYMBOL0)
        if (_bit_index < 2) { 
            if (_last_symbol == 0 && current_symbol == 1) {
                _bit_index++; // Sync bit valid, proceed
                return; 
            } else {
                _state = HT600_STATE::IDLE;
                return;
            }
        }

        // --- DATA BITS DECODING (Bit 2 to 19) ---
        uint8_t byte_idx = _bit_index >> 3;
        uint8_t bit_mask = (1 << (_bit_index & 0x07));

        if (_last_symbol == 0 && current_symbol == 0) {
            // Logical '0': SYMBOL0 + SYMBOL0
            _buffer_HL[byte_idx] &= ~bit_mask;
            _buffer_Z[byte_idx]  &= ~bit_mask;
        }
        else if (_last_symbol == 1 && current_symbol == 1) {
            // Logical '1': SYMBOL1 + SYMBOL1
            _buffer_HL[byte_idx] |= bit_mask;
            _buffer_Z[byte_idx]  &= ~bit_mask;
        }
        else if (_last_symbol == 1 && current_symbol == 0) {
            // Logical 'Z': SYMBOL1 + SYMBOL0
            _buffer_HL[byte_idx] &= ~bit_mask;
            _buffer_Z[byte_idx]  |= bit_mask;
        }
        else {
            _state = HT600_STATE::IDLE;
            return;
        }

        _bit_index++; 
        // 2 Sync bits + 18 Data bits = 20 total bits
        if (_bit_index >= 20) {
            _state = HT600_STATE::DONE;
        }
    }
}

/**
 * @brief Extracts the first 16 decoded data bits (bit 17 and 18 are always dummy).
 * @param z_mapping_value Logical value to assign if a bit is 'Z'.
 * @return A uint16_t containing the 16 bits of information.
 */
uint16_t HT600::get_HL_data(bool z_mapping_value) const {
    uint16_t result = 0;

    for (uint8_t i = 0; i < 16; i++) {
        // Avoid the first two bits (SYNC)
        uint8_t internal_idx = i + 2; 
        uint8_t byte_idx = internal_idx >> 3;
        uint8_t bit_mask = (1 << (internal_idx & 0x07));

        // Boolean extraction
        bool bit_hl = (_buffer_HL[byte_idx] & bit_mask);
        bool bit_z  = (_buffer_Z[byte_idx] & bit_mask);

        if (bit_z) {
            if (z_mapping_value) result |= (1 << i);
        } else {
            if (bit_hl) result |= (1 << i);
        }
    }
    return result;
}

/**
 * @brief Extracts the High-Z status for the first 16 bits.
 * @param z_value If true, returns 1 for Z bits and 0 for defined bits.
 * If false, returns 0 for Z bits and 1 for defined bits (inverted).
 * @return A uint16_t mask representing the trinary 'Open' states.
 */
uint16_t HT600::get_Z_data(bool z_value) const {
    uint16_t result = 0;

    for (uint8_t i = 0; i < 16; i++) {
        // Avoid the first two bits (SYNC)
        uint8_t internal_idx = i + 2; 
        uint8_t byte_idx = internal_idx >> 3;
        uint8_t bit_mask = (1 << (internal_idx & 0x07));

        // Check if the bit is 'Z'
        bool is_z = (_buffer_Z[byte_idx] & bit_mask);

        // If z_value is true, we want Z bits to be 1 and defined bits to be 0. 
        // If z_value is false, we want the opposite.
        if (!(is_z ^ z_value)) {
            result |= (1 << i);
        }
    }
    return result;
}

void HT600::reset() {
    _state = HT600_STATE::IDLE;
    _bit_index = 0;
    _half_symbol_read = false;
    _last_symbol = false;
    _last_interrupt_tick = 0;
    _period_L = 0;
    _period_H = 0;

    // Clear buffers
    for (int i = 0; i < 3; i++) {
        _buffer_HL[i] = 0;
        _buffer_Z[i] = 0;
    }
}
