#include "tachometer.h"

// Static variable definitions
volatile unsigned int Tachometer::tachCount = 0;
volatile unsigned int Tachometer::rpm = 0;

Tachometer::Tachometer() : lastRpmCalcTime(0) {
}

void Tachometer::begin() {
    pinMode(TACH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
    Serial.println("Tachometer initialized");
}

void Tachometer::calculateRPM() {
    noInterrupts();
    unsigned int count = tachCount;
    tachCount = 0;
    interrupts();

    // Calculate RPM - using fixed time interval for more stable readings
    rpm = count * 30;
    
    // Log RPM via Serial
    Serial.print("RPM: ");
    Serial.print(rpm);
    Serial.print(" | count: ");
    Serial.println(count);
    
    lastRpmCalcTime = millis();
}

unsigned int Tachometer::getRPM() const {
    return rpm;
}

bool Tachometer::shouldCalculateRPM() const {
    return (millis() - lastRpmCalcTime >= RPM_CALC_INTERVAL);
}

void Tachometer::tachISR() {
    tachCount++;
}
