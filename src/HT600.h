#ifndef HT600_H
#define HT600_H

// Pure C++!
#include <stdint.h>
#include <stdbool.h>
// ESP32 and ESP8266 have a different way of defining RAM functions (IRAM_ATTR) compared to AVR, STM32, ecc.
#if defined(ESP32) || defined(ESP8266)
  #include <Arduino.h> // We need to include the Arduino header for the IRAM_ATTR definition
#else
  #if !defined(IRAM_ATTR)
    #define IRAM_ATTR  // No-op for non-ESP platforms
  #endif
#endif


// Oscillator frequency at 12V.
// The higher the frequency, the more sensitive it is to the supply voltage.
#define HT680_120K_FOSC 265  // ~265 KHz
#define HT680_150K_FOSC 215  // ~215 KHz
#define HT680_180K_FOSC 180  // ~180 KHz
#define HT680_220K_FOSC 150  // ~150 KHz
#define HT680_270K_FOSC 120  // ~120 KHz
#define HT680_330K_FOSC 100  //  100 KHz (Reference value recommended by the datasheet)
#define HT680_390K_FOSC 85   // ~85 KHz
#define HT680_470K_FOSC 70   // ~70 KHz
#define HT680_560K_FOSC 60   // ~60 KHz
#define HT680_680K_FOSC 50   // ~50 KHz
#define HT680_820K_FOSC 40   // ~40 KHz
#define HT680_1M0_FOSC  33   // ~33 KHz
#define HT680_1M5_FOSC  22   // ~22 KHz
#define HT680_2M0_FOSC  16   // ~16 KHz

// Tollerance of 30% is a good compromise
#define HT600_TOLERANCE      0.3

#define HT600_IS_IN_RANGE(val, min, max) (val >= min && val <= max)

/**
 * @section HT680/318 SERIES
 * According to the datasheet, each word handles a total of 18 bits of information.
 * The 18 bits are divided into 'N' address bits and '18-N' data bits.
 * Unused bits or unbonded pins are internally set to 'Z' (Floating/High Impedance).
 *
 * --- CHIP SPECIFIC CONFIGURATIONS ---
 * HT600:  9 Address, 5 Addr/Data.  (Fixed 'Z' on A5, A10, AD17)
 * HT680:  8 Address, 4 Addr/Data.  (Fixed 'Z' on A4, A5, A10, AD16, AD17)
 * HT6207: 10 Address, 4 Data bits. (Fixed 'Z' on A5, A10, D16, D17)
 *
 * --- TRIGGER TYPES ---
 * 1. TE (Transmit Enable): Used by HT600/HT680. Transmission starts when TE is HIGH.
 * Address/Data pins can be used as fixed address bits or switches.
 * 2. DATA Trigger: Used by HT6207. Transmission starts when any data pin is HIGH.
 * 
 * @section INFORMATION WORD
 * ┆<─────── Pilot ──────>┆   ┆<──────────────────── Sync ────────────────────>|<──────── Address Code ────────>|<───── Data Code ────>|
 * ┆       (6 bits)       ┆   ┆                       ┆                        ┆            (11 bits)           ┆        (7 bits)      ┆
 * ┆                      ├───┤   ┌───────┐       ┌───┤   ┌───────┐       ┌────┼────────────────────────────────┼──────────────────────┤
 * ┆                      │   │   │       │       │   │   │       │       │    │╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳┆╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳│              
 *─┴──────────────────────┘   └───┘       └───────┘   └───┘       └───────┘    └────────────────────────────────┴──────────────────────┴────────

 * @section ADDRESS/DATA WAVEFORM
 *           ┆←-------- bit --------→┆
 *           ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
 * fosc/33: ─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └──
 *           ┆           ┆           ┆
 *           ┆       ┌───┐       ┌───┐
 * '1'    : ─┆───────┘   └───────┘   └────  (Symbol 1 + Symbol 1)
 *           ┆           ┆           ┆
 *           ┆   ┌───────┐   ┌───────┐ 
 * '0'    : ─┆───┘       └───┘       └────  (Symbol 0 + Symbol 0)
 *           ┆           ┆           ┆
 *           ┆       ┌───┐   ┌───────┐ 
 * 'Z'    : ─┆───────┘   └───┘       └────  (Symbol 1 + Symbol 0)
 *           ┆           ┆           ┆
 *           ┆   ┌───────┐       ┌───┐ 
 * 'SYNC' : ─┆───┘       └───────┘   └────  (Symbol 0 + Symbol 1)
 *           ┆← SYMBOL  →┆←  SYMBOL →┆
 * 
 * @section SYMBOLS FORMAT
 *              ┆   ┌───────┤ 
 * 'Symbol 0': ─┼───┘       ├──
 *              ┆           ┆         
 *              ┆       ┌───┤  
 * 'Symbol 1': ─┼───────┘   ├──
*/        

enum class HT600_STATE {
    IDLE,
    READING,
    DONE
};


class HT600 {
    public:
        HT600(const uint16_t fosc_khz, const float tolerance, const uint16_t tick_length_us, const uint16_t noise_filter_us);
        const bool available() { return _state == HT600_STATE::DONE; } ;
        const HT600_STATE getState() { return _state; };
        uint16_t getReceivedValue(bool z_value = 0) const;
        uint16_t getTristateValue (bool z_value = 1) const;
        void resetAvailable();
        void IRAM_ATTR handleInterrupt(const bool pinState, const uint32_t ticks);

    private:
        uint16_t _short_tick_min; 
        uint16_t _short_tick_max;
        uint16_t _long_tick_min;  
        uint16_t _long_tick_max; 
        uint16_t _pilot_tick_min;
        uint16_t _pilot_tick_max;
        uint16_t _noise_filter_tick;

        HT600_STATE _state = HT600_STATE::IDLE;

        // Since the HT600 is a ternary encoder, we can use 2 bits to represent the 3 possible states of each bit (0, 1, Z).
        // Simplest way to do this is to use two different buffer, one to store if the bit is '1' and the other to store if the bit is 'Z'.
        // Since we have 18 bits we need 3 bytes for each buffer
        volatile uint8_t _buffer_HL [3]; // In this buffer we store the state of the 'H' and 'L' bits
        volatile uint8_t _buffer_Z  [3]; // In this buffer we store the state of the 'Z' bit

        volatile uint8_t _bit_index = 0; // Index of the current bit being read (0-17)
        volatile bool _half_symbol_read = false; // Flag to indicate if we have read the first half of the symbol 
        volatile bool _last_symbol = false;

        volatile uint32_t _last_interrupt_tick = 0; // Last time the interrupt was called
        volatile uint16_t _period_L = 0; // Duration of the last LOW period in ticks
        volatile uint16_t _period_H = 0; // Duration of the last HIGH period in ticks
        

        
};


#endif