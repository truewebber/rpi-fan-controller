#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

// Forward declaration to avoid circular dependency
class TemperatureSensor;

class FanController {
private:
    int currentPwmValue;
    
    // Calculate fan speed based on temperature with parabolic curve
    int calculateFanSpeed(float temp, float minTemp, float maxTemp) const;

public:
    FanController();
    
    // Initialize fan controller
    void begin();
    
    // Update fan speed based on temperature data
    void updateFanSpeed(const TemperatureSensor& tempSensor);
    
    // Set fan speed directly (for testing or manual control)
    void setFanSpeed(int pwmValue);
    
    // Get current PWM value
    int getCurrentPwmValue() const;
    
    // Get current fan speed as percentage
    int getCurrentSpeedPercent() const;
};

#endif // FAN_CONTROLLER_H
