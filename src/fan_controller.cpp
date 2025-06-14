#include "fan_controller.h"
#include "temperature_sensor.h"  // Include after forward declaration
#include <math.h>

FanController::FanController() : currentPwmValue(FAN_SPEED_MIN) {
}

void FanController::begin() {
    pinMode(FAN_PWM_PIN, OUTPUT);
    currentPwmValue = FAN_SPEED_MIN;
    analogWrite(FAN_PWM_PIN, currentPwmValue);
    Serial.println("Fan controller initialized");
}

int FanController::calculateFanSpeed(float temp, float minTemp, float maxTemp) const {
    if (temp <= minTemp) {
        return FAN_SPEED_MIN;
    } else if (temp >= maxTemp) {
        return FAN_SPEED_MAX;
    } else {
        // Parabolic curve for more aggressive cooling at higher temperatures
        float tempRatio = (temp - minTemp) / (maxTemp - minTemp);
        float curvedRatio = pow(tempRatio, FAN_CURVE_EXPONENT);
        return FAN_SPEED_MIN + curvedRatio * (FAN_SPEED_MAX - FAN_SPEED_MIN);
    }
}

void FanController::updateFanSpeed(const TemperatureSensor& tempSensor) {
    float highestCpuTemp, highestNvmeTemp;
    tempSensor.getHighestTemperatures(highestCpuTemp, highestNvmeTemp);
    
    // If no temperature data is available at all, set fan to minimum speed
    if (!tempSensor.hasTemperatureData()) {
        currentPwmValue = FAN_SPEED_MIN;
        analogWrite(FAN_PWM_PIN, currentPwmValue);
        Serial.println("No temperature data available. Fan set to minimum speed.");
        return;
    }
    
    // Calculate fan speed based on CPU temperature
    int cpuFanSpeed = calculateFanSpeed(highestCpuTemp, CPU_TEMP_MIN, CPU_TEMP_MAX);
    
    // Calculate fan speed based on NVME temperature  
    int nvmeFanSpeed = calculateFanSpeed(highestNvmeTemp, NVME_TEMP_MIN, NVME_TEMP_MAX);
    
    // Use the higher of the two calculated fan speeds
    int newPwmValue = max(cpuFanSpeed, nvmeFanSpeed);
    
    // Only update if the value has changed
    if (newPwmValue != currentPwmValue) {
        currentPwmValue = newPwmValue;
        analogWrite(FAN_PWM_PIN, currentPwmValue);
        
        // Log fan speed change
        Serial.print("Fan speed updated - PWM: ");
        Serial.print(currentPwmValue);
        Serial.print(" (");
        Serial.print(getCurrentSpeedPercent());
        Serial.print("%) | Based on CPU: ");
        Serial.print(highestCpuTemp);
        Serial.print("°C, NVME: ");
        Serial.print(highestNvmeTemp);
        Serial.print("°C");
        
        // Show status of connected/disconnected devices
        Serial.print(" | Devices: ");
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (i > 0) Serial.print(",");
            Serial.print(i + 1);
            Serial.print(":");
            if (tempSensor.isDeviceConnected(i)) {
                Serial.print("ON");
            } else {
                TemperatureData deviceTemp = tempSensor.getDeviceTemperature(i);
                if (deviceTemp.isValid && (deviceTemp.cpuTemp > 0.0 || deviceTemp.nvmeTemp > 0.0)) {
                    Serial.print("OFF(saved)");
                } else {
                    Serial.print("OFF");
                }
            }
        }
        Serial.println();
    }
}

void FanController::setFanSpeed(int pwmValue) {
    // Constrain PWM value to valid range
    pwmValue = constrain(pwmValue, 0, 255);
    
    if (pwmValue != currentPwmValue) {
        currentPwmValue = pwmValue;
        analogWrite(FAN_PWM_PIN, currentPwmValue);
        
        Serial.print("Fan speed manually set to PWM: ");
        Serial.print(currentPwmValue);
        Serial.print(" (");
        Serial.print(getCurrentSpeedPercent());
        Serial.println("%)");
    }
}

int FanController::getCurrentPwmValue() const {
    return currentPwmValue;
}

int FanController::getCurrentSpeedPercent() const {
    return map(currentPwmValue, 0, 255, 0, 100);
}
