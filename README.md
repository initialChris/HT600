# HT600 Series Trinary Decoder

**A lightweight, high-performance, and hardware-agnostic C++ library for decoding trinary RF signals from Holtek HT600, HT680, and HT6207 encoders.**

While developed to be cross-platform, it has been specifically tested on **Arduino Duemilanove** and **ATtiny45**.

## Key Features

* **Hardware Agnostic:** Decoupled from hardware timers and interrupts. You simply feed it the pin state and a timestamp (ticks/micros).
* **Trinary Logic Support:** Fully supports `0`, `1`, and `Open` (High-Z) states.
* **Highly Configurable:** Adjustable oscillator frequency, tolerance, and noise filtering.
* **Memory Efficient:** Optimized to run on chips with very limited RAM (e.g., ATtiny series).

## Compatible Hardware

The library supports the standard [Holtek 3^18 series encoders](ht600_datasheet.pdf). Although the physical pinouts differ, the data structure is identical.

**General Specifications:**
* **Operating Voltage:** 2.4V ~ 12V.
* **Oscillator:** Determined by a single external resistor. Standard is **100kHz** with a **330kΩ** resistor between OSC1 and OSC2.

### Chip Differences

| Chip | Address Bits | Data Bits | Transmission Trigger |
| :--- | :--- | :--- | :--- |
| **HT600** | 9 (A0 A1 A2 A3 A4 - A6 A7 A8 A9 -) | 5 (AD11 AD12 AD13 AD14 AD15) | **TE** Pin (Active HIGH) |
| **HT680** | 8 (A0 A1 A2 A3 - - A6 A7 A8 A9 -) | 4 (AD11 AD12 - AD14 AD15 ) | **TE** Pin (Active HIGH) |
| **HT6207**| 10 (A0 A1 A2 A3 A4 - A6 A7 A8 A9 - A11)| 4 (D12 D13 D14 D15 ) | Any **Data** Pin (Active HIGH) |

> **Note on Packet Size:** The physical protocol transmits 18 bits. However, this library decodes and stores only the **first 16 bits**. The last two bits are ignored as they are "dummy" bits in standard packages and contain no usable information.

## Protocol Structure

The signal consists of a Pilot period, a Sync pattern, followed by the Address/Data payload.
```text
    ┆<─────── Pilot ──────>┆   ┆<──────────────────── Sync ────────────────────>|<──────── Address Code ────────>|<───── Data Code ────>|
    ┆       (6 bits)       ┆   ┆                       ┆                        ┆            (11 bits)           ┆        (7 bits)      ┆
    ┆                      ├───┤   ┌───────┐       ┌───┤   ┌───────┐       ┌────┼────────────────────────────────┼──────────────────────┤
    ┆                      │   │   │       │       │   │   │       │       │    │╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳┆╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳╳│              
 ───┴──────────────────────┘   └───┘       └───────┘   └───┘       └───────┘    └────────────────────────────────┴──────────────────────┴────────
```

The protocol uses PWM-like symbols. A logical bit is composed of two symbols.
```text
            ┆   ┌───────┤ 
Symbol 0': ─┼───┘       ├── (Short Low / Long High)
            ┆           ┆         
            ┆       ┌───┤  
Symbol 1': ─┼───────┘   ├── (Long Low / Short High)
```

By combining two symbols, we define the logic states:
```text
          ┆←-------- bit --------→┆
          ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
fosc/33: ─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └──
          ┆           ┆           ┆
          ┆       ┌───┐       ┌───┐
1'    :  ─┆───────┘   └───────┘   └────  (Symbol 1 + Symbol 1)
          ┆           ┆           ┆
          ┆   ┌───────┐   ┌───────┐ 
0'    :  ─┆───┘       └───┘       └────  (Symbol 0 + Symbol 0)
          ┆           ┆           ┆
          ┆       ┌───┐   ┌───────┐ 
Z'    :  ─┆───────┘   └───┘       └────  (Symbol 1 + Symbol 0)
          ┆           ┆           ┆
          ┆   ┌───────┐       ┌───┐ 
SYNC' :  ─┆───┘       └───────┘   └────  (Symbol 0 + Symbol 1)
          ┆← SYMBOL  →┆←  SYMBOL →┆
```

## Configuration & Tuning

To ensure reliable decoding, the library must be configured to match the hardware transmitter. The frequency decreases as battery voltage drops, so a generous tolerance is recommended (but do not overdo it to avoid overlapping in the recognition of the short and long periods).

### Resistor to Frequency & Timing Table (at 12V)
Use this table to find the correct timing values. The **Short Pulse** corresponds to the base time unit $(T = 1 / (F_{osc} / 33))$.

| Resistor | Macro | Freq ($F_{osc}$) | Short ( $1T$ ) | Long ( $2T$ ) | Pilot ( $36T$ ) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 120 kΩ | `HT680_120K_FOSC` | ~265 KHz | 125 μs | 250 μs | 4.50 ms |
| 150 kΩ | `HT680_150K_FOSC` | ~215 KHz | 153 μs | 306 μs | 5.50 ms |
| 180 kΩ | `HT680_180K_FOSC` | ~180 KHz | 183 μs | 366 μs | 6.58 ms |
| 220 kΩ | `HT680_220K_FOSC` | ~150 KHz | 220 μs | 440 μs | 7.92 ms |
| 270 kΩ | `HT680_270K_FOSC` | ~120 KHz | 275 μs | 550 μs | 9.90 ms |
| **330 kΩ** | **`HT680_330K_FOSC`** | **100 KHz** | **330 μs** | **660 μs** | **11.88 ms** |
| 390 kΩ | `HT680_390K_FOSC` | ~85 KHz | 388 μs | 776 μs | 13.96 ms |
| 470 kΩ | `HT680_470K_FOSC` | ~70 KHz | 471 μs | 942 μs | 16.95 ms |
| 560 kΩ | `HT680_560K_FOSC` | ~60 KHz | 550 μs | 1100 μs | 19.80 ms |
| 680 kΩ | `HT680_680K_FOSC` | ~50 KHz | 660 μs | 1320 μs | 23.76 ms |
| 820 kΩ | `HT680_820K_FOSC` | ~40 KHz | 825 μs | 1650 μs | 29.70 ms |
| 1.0 MΩ | `HT680_1M0_FOSC` | ~33 KHz | 1000 μs | 2000 μs | 36.00 ms |
| 1.5 MΩ | `HT680_1M5_FOSC` | ~22 KHz | 1500 μs | 3000 μs | 54.00 ms |
| 2.0 MΩ | `HT680_2M0_FOSC` | ~16 KHz | 2062 μs | 4125 μs | 74.23 ms |

### Constructor Parameters

1. **Fosc (kHz)**: The oscillator frequency (usually determined by the resistor).
2. **Tolerance**: Percentage (e.g., 0.3 for 30%) to account for voltage drops/component variance.
3. **Tick Resolution**: The resolution of your timestamp in microseconds (usually 1.0 if using micros()).
4. **Noise Filter**: Minimum pulse width to accept (in microseconds).

## Usage Example (Arduino/PlatformIO)
```cpp
#include <HT600.h>

#define RF_PIN 2

// 1. Setup the decoder
// Freq: 100kHz, Tolerance: 30%, Tick: 1us, Filter: 50us
HT600 decoder(HT680_330K_FOSC, 0.3f, 1, 50);

// 2. Interrupt Service Routine
void IRAM_ATTR handleInterrupt() {
    // Feed the decoder with state and timestamp
    decoder.handleInterrupt(digitalRead(RF_PIN), micros());
}

void setup() {
    Serial.begin(115200);
    pinMode(RF_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RF_PIN), handleInterrupt, CHANGE);
}

void loop() {
    // 3. Check if packet is ready
    if (decoder.getState() == HT600_STATE::DONE) {
        
        // Get Data (Mapping Z to 0)
        uint16_t data = decoder.getReceivedValue();
        // Get Z-Mask (1 where the bit is Floating/Open)
        uint16_t z_mask = decoder.getTristateValue();

        Serial.print("Data: "); Serial.println(data, BIN);
        Serial.print("Z-Mask: "); Serial.println(z_mask, BIN);

        decoder.resetAvailable();
    }
}
```