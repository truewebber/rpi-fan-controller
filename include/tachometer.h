#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>
#include "config.h"

class Tachometer {
private:
    static volatile unsigned int tachCount;
    static volatile unsigned int rpm;
    unsigned long lastRpmCalcTime;

public:
    Tachometer();
    
    // Initialize tachometer (setup interrupt)
    void begin();
    
    // Calculate RPM based on tach count
    void calculateRPM();
    
    // Get current RPM value
    unsigned int getRPM() const;
    
    // Check if it's time to calculate RPM
    bool shouldCalculateRPM() const;
    
    // Interrupt service routine (must be static)
    static void tachISR();
};

#endif // TACHOMETER_H
