/**
 * HT600/HT680 Universal Scanner Example
 * * This sketch demonstrates how to use the HT600 library to decode
 * trinary signals (0, 1, Open/Z). It includes a software debounce
 * filter to prevent serial flooding and formats the output for easy reading.
 */

#include <Arduino.h>
#include <HT600.h>

// --- HARDWARE CONFIGURATION ---
// Receiver data pin (Must be an interrupt-capable pin)
// On Arduino Uno/Nano: Pin 2 or 3. On ESP32: Any GPIO.
#define RF_PIN 2           

// Visual feedback when a valid signal is received
#define STATUS_LED LED_BUILTIN 
#define REPEAT_TX_RATE 300 //ms
// --- DECODER SETTINGS ---
// 1. Oscillator Frequency: 390K resistor -> approx 85kHz (Use macros for other resistors)
// 2. Tolerance: 30% (0.3f) to account for voltage fluctuations
// 3. Tick Resolution: 1us (matches micros() resolution)
// 4. Noise Filter: 50us (ignore pulses shorter than this)
HT600 decoder(HT680_390K_FOSC, 0.3f, 1, 50);

// --- GLOBAL STATE MANAGEMENT ---
// Struct to hold the state of the last received packet for debouncing
struct RxState {
    uint16_t last_data;
    uint32_t last_time;
    bool active;
    bool last_active;
    uint16_t current_data;
    uint16_t z_mask;
} rx_state;

// --- INTERRUPT SERVICE ROUTINE (ISR) ---
// IRAM_ATTR places this function in RAM for faster execution (critical for ESP32)
void IRAM_ATTR handleInterrupt() {
    // Feed the decoder with the current pin state and timestamp
    decoder.handleInterrupt(digitalRead(RF_PIN), micros());
}

// --- HELPER FUNCTIONS ---

/**
 * Prints a 16-bit value to Serial with nibble spacing (e.g., "1010 0101").
 * If 'is_tristate' is true, it combines the Data and Z-Mask to print '0', '1', or 'Z'.
 */
void printFormatted(uint16_t data_val, uint16_t z_mask, bool as_tristate) {
    for (int i = 15; i >= 0; i--) {
        if (as_tristate) {
            // Logic: If the Z-mask bit is 1, the pin was Floating (Z).
            // Otherwise, it was strictly High (1) or Low (0).
            if (bitRead(z_mask, i)) {
                Serial.print('Z');
            } else {
                Serial.print(bitRead(data_val, i) ? '1' : '0');
            }
        } else {
            // Simple binary dump
            Serial.print(bitRead(data_val, i));
        }

        // Add a space every 4 bits for readability
        if (i % 4 == 0 && i != 0) Serial.print(' ');
    }
}

void setup() {
    // Initialize Serial for debug output
    Serial.begin(115200);
    
    // Configure pins
    pinMode(RF_PIN, INPUT);
    pinMode(STATUS_LED, OUTPUT);
    
    Serial.println(F("\n=== HT600/HT680 Trinary Scanner ==="));
    Serial.println(F("Waiting for RF signals..."));
    
    // Attach interrupt to trigger on ANY state change (Rising or Falling)
    attachInterrupt(digitalPinToInterrupt(RF_PIN), handleInterrupt, CHANGE);
}

void loop() {

    if (millis() - rx_state.last_time > REPEAT_TX_RATE){
      rx_state.active = false;
    }

    if (decoder.available()) {
      // Visual Feedback: Turn LED on
      digitalWrite(STATUS_LED, HIGH);

      // Retrieve decoded data and the Z-mask (High-Impedance map)
      uint16_t current_data = decoder.getReceivedValue(true); // Map 'Z' bits to '1' (use false to map 'Z' bits to '0')
      uint16_t z_mask = decoder.getTristateValue(true);         // '1' indicates a 'Z' bit (use false to invert the representation)
      uint32_t now = millis();

      // --- SPAM FILTER / DEBOUNCE ---
      // Ignore the packet if it's identical to the previous one 
      // AND received within 500ms.
      rx_state.active = true;
      rx_state.last_time = now;
      rx_state.current_data = current_data;
      rx_state.z_mask = z_mask;

      
      // Reset the FSM to IDLE state to listen for the next pilot signal
      decoder.resetAvailable();

      // Turn LED off
      digitalWrite(STATUS_LED, LOW); 
    }

    if ((rx_state.active != rx_state.last_active) || (rx_state.last_data != rx_state.current_data)){
      rx_state.last_active = rx_state.active;
      rx_state.last_data = rx_state.current_data;
      if (rx_state.active){
        // --- SERIAL OUTPUT ---
        Serial.print(F("[RECV] Raw Bin:  "));
        printFormatted(rx_state.current_data, 0, false);
        Serial.print(F(" (0x"));
        Serial.print(rx_state.current_data, HEX);
        Serial.print(F(") | Tristate: "));
        printFormatted(rx_state.current_data, rx_state.z_mask, true);
        Serial.print(F(" (0x"));
        Serial.print(rx_state.z_mask, HEX);
        Serial.println(F(")"));
      }
    }
}