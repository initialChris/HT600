#include <Arduino.h>
#include <HT600.h>

// Receiver pin (On Atmega328P, D2 is interrupt on channel 0)
#define RF_PIN 2

// -- Decoder settings --
// 390K resistor
// 20% of tollerance
// 1us tick length (so we can use micros() as timestamp)
// 50us noise filter
HT600 decoder(HT680_390K_FOSC, 20, 1, 50);

// Interrupt Service Routine (ISR)
void IRAM_ATTR handleInterrupt() {
    // Passiamo lo stato attuale del pin e il timestamp in microsecondi
    decoder.processEvent(digitalRead(RF_PIN), micros());
}

void setup() {
    Serial.begin(115200);
    pinMode(RF_PIN, INPUT);
    pinMode(13, OUTPUT);
    Serial.println(F("--- HT680 PlatformIO Scanner ---"));
    
    // Enable interrupt on the receiver pin
    attachInterrupt(digitalPinToInterrupt(RF_PIN), handleInterrupt, CHANGE);
}

uint16_t last_data = 0;
uint32_t last_time = 0;
void loop() {

    // If decoder have data, we can read it

    if (decoder.available()) {
        
        
        uint16_t data = decoder.get_HL_data(true);
        uint16_t z_mask = decoder.get_Z_data(true);

        // Avoid spamming repeated data
        if (last_data == data && millis() - last_time < 500) {
          decoder.reset();
          return;
        } 

        last_data = data;
        last_time = millis();

        Serial.print(F("Data (BIN): "));
        for (int i = 15; i >= 0; i--) {
            Serial.print(bitRead(data, i));
            if (i % 4 == 0 && i != 0) Serial.print(" ");
        }

        Serial.print(F("\nZ mask:     "));
        for (int i = 15; i >= 0; i--) {
            Serial.print(bitRead(z_mask, i));
            if (i % 4 == 0 && i != 0) Serial.print(" ");
        }
        Serial.println();
        Serial.print(F("Tristate:   "));
        for (int i = 15; i >= 0; i--) {
          if (bitRead(z_mask, i) )
            Serial.print(F("Z"));
          else 
            Serial.print(bitRead(data, i) ? F("1") : F("0"));
          
          if (i % 4 == 0 && i != 0) Serial.print(" ");
        }
        Serial.println("\n");

        // Reset obbligatorio per rimettere la FSM in ascolto del prossimo Pilot
        decoder.reset();
    }
}