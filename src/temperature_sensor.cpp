#include "temperature_sensor.h"
#include "fan_controller.h"  // Include after forward declaration

TemperatureSensor::TemperatureSensor() : fanController(nullptr) {
    for (int i = 0; i < NUM_DEVICES; i++) {
        deviceTemps[i] = {0.0, 0.0, false, 0};
        deviceConnected[i] = false;
        missedPolls[i] = 0;
    }
}

void TemperatureSensor::begin() {
    Serial.println("Temperature sensor initialized");
}

void TemperatureSensor::setFanController(FanController* fc) {
    fanController = fc;
}

bool TemperatureSensor::parseTemperatureData(int deviceId, const String& data) {
    if (deviceId < 0 || deviceId >= NUM_DEVICES) {
        return false;
    }
    
    // Expected format: CPU:xx.x|NVME:xx.x
    int cpuPos = data.indexOf("CPU:");
    int nvmePos = data.indexOf("|NVME:");
    
    if (cpuPos != -1 && nvmePos != -1) {
        // Extract CPU temperature
        String cpuTempStr = data.substring(cpuPos + 4, nvmePos);
        float cpuTemp = cpuTempStr.toFloat();
        
        // Extract NVME temperature
        String nvmeTempStr = data.substring(nvmePos + 6);
        float nvmeTemp = nvmeTempStr.toFloat();
        
        // Update device data
        deviceTemps[deviceId].cpuTemp = cpuTemp;
        deviceTemps[deviceId].nvmeTemp = nvmeTemp;
        deviceTemps[deviceId].isValid = true;
        deviceTemps[deviceId].lastUpdateTime = millis();
        
        deviceConnected[deviceId] = true;
        missedPolls[deviceId] = 0;
        
        // Log the parsed temperatures
        Serial.print("Device ");
        Serial.print(deviceId + 1);
        Serial.print(" temperatures - CPU: ");
        Serial.print(cpuTemp);
        Serial.print("°C, NVME: ");
        Serial.print(nvmeTemp);
        Serial.println("°C");
        
        // IMPORTANT: Update fan speed immediately based on new temperature data
        if (fanController) {
            fanController->updateFanSpeed(*this);
        }
        
        return true;
    }
    
    return false;
}

TemperatureData TemperatureSensor::getDeviceTemperature(int deviceId) const {
    if (deviceId >= 0 && deviceId < NUM_DEVICES) {
        return deviceTemps[deviceId];
    }
    return {0.0, 0.0, false, 0};
}

void TemperatureSensor::getHighestTemperatures(float& highestCpu, float& highestNvme) const {
    highestCpu = 0.0;
    highestNvme = 0.0;
    
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (deviceTemps[i].isValid) {
            if (deviceTemps[i].cpuTemp > highestCpu) {
                highestCpu = deviceTemps[i].cpuTemp;
            }
            if (deviceTemps[i].nvmeTemp > highestNvme) {
                highestNvme = deviceTemps[i].nvmeTemp;
            }
        }
    }
}

bool TemperatureSensor::hasTemperatureData() const {
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (deviceTemps[i].isValid && (deviceTemps[i].cpuTemp > 0.0 || deviceTemps[i].nvmeTemp > 0.0)) {
            return true;
        }
    }
    return false;
}

void TemperatureSensor::handleMissedPoll(int deviceId) {
    if (deviceId >= 0 && deviceId < NUM_DEVICES) {
        missedPolls[deviceId]++;
        
        Serial.print("Device ");
        Serial.print(deviceId + 1);
        Serial.print(" missed polls: ");
        Serial.println(missedPolls[deviceId]);
        
        if (missedPolls[deviceId] >= MAX_MISSED_POLLS && deviceConnected[deviceId]) {
            deviceConnected[deviceId] = false;
            Serial.print("Device ");
            Serial.print(deviceId + 1);
            Serial.println(" disconnected (too many missed polls)");
            
            // IMPORTANT: Update fan speed when a device disconnects
            if (fanController) {
                fanController->updateFanSpeed(*this);
            }
        }
    }
}

bool TemperatureSensor::isDeviceConnected(int deviceId) const {
    if (deviceId >= 0 && deviceId < NUM_DEVICES) {
        return deviceConnected[deviceId];
    }
    return false;
}

void TemperatureSensor::printTemperatureSummary() const {
    Serial.println("=== Temperature Summary ===");
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (deviceConnected[i]) {
            Serial.print("Device ");
            Serial.print(i + 1);
            Serial.print(": CPU=");
            Serial.print(deviceTemps[i].cpuTemp);
            Serial.print("°C, NVME=");
            Serial.print(deviceTemps[i].nvmeTemp);
            Serial.print("°C, missed=");
            Serial.println(missedPolls[i]);
        } else {
            Serial.print("Device ");
            Serial.print(i + 1);
            Serial.print(": Not connected (missed=");
            Serial.print(missedPolls[i]);
            Serial.print(", last CPU=");
            Serial.print(deviceTemps[i].cpuTemp);
            Serial.print("°C, last NVME=");
            Serial.print(deviceTemps[i].nvmeTemp);
            Serial.println("°C)");
        }
    }
}

void TemperatureSensor::resetMissedPolls(int deviceId) {
    if (deviceId >= 0 && deviceId < NUM_DEVICES) {
        missedPolls[deviceId] = 0;
    }
}
