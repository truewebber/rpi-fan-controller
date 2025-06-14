#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include "config.h"

struct TemperatureData {
    float cpuTemp;
    float nvmeTemp;
    bool isValid;
    unsigned long lastUpdateTime;
};

class TemperatureSensor {
private:
    TemperatureData deviceTemps[NUM_DEVICES];
    bool deviceConnected[NUM_DEVICES];
    int missedPolls[NUM_DEVICES];

public:
    TemperatureSensor();
    
    // Initialize temperature sensor
    void begin();
    
    // Parse temperature data from device response
    bool parseTemperatureData(int deviceId, const String& data);
    
    // Get temperature data for specific device
    TemperatureData getDeviceTemperature(int deviceId) const;
    
    // Get highest temperatures across all devices
    void getHighestTemperatures(float& highestCpu, float& highestNvme) const;
    
    // Check if any temperature data is available
    bool hasTemperatureData() const;
    
    // Handle missed poll for device
    void handleMissedPoll(int deviceId);
    
    // Check if device is connected
    bool isDeviceConnected(int deviceId) const;
    
    // Print temperature summary
    void printTemperatureSummary() const;
    
    // Reset missed polls counter for device
    void resetMissedPolls(int deviceId);
};

#endif // TEMPERATURE_SENSOR_H
