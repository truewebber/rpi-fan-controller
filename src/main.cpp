#include <Arduino.h>
#include "config.h"
#include "tachometer.h"
#include "temperature_sensor.h"
#include "fan_controller.h"
#include "device_communication.h"

// Global objects
Tachometer tachometer;
TemperatureSensor tempSensor;
FanController fanController;
DeviceCommunication deviceComm;

void setup() {
    // Initialize serial communication
    Serial.begin(9600);
    
    // Initialize all modules
    tachometer.begin();
    tempSensor.begin();
    fanController.begin();
    
    // IMPORTANT: Set up cross-module communication
    // Temperature sensor will immediately update fan when new data arrives
    tempSensor.setFanController(&fanController);
    
    deviceComm.begin(&tempSensor);
    
    // Log system initialization
    Serial.println("System Initialized.");
    Serial.println("Automatic fan control enabled with the following thresholds:");
    Serial.print("CPU: ");
    Serial.print(CPU_TEMP_MIN);
    Serial.print("°C - ");
    Serial.print(CPU_TEMP_MAX);
    Serial.println("°C");
    Serial.print("NVME: ");
    Serial.print(NVME_TEMP_MIN);
    Serial.print("°C - ");
    Serial.print(NVME_TEMP_MAX);
    Serial.println("°C");
    Serial.print("Fan curve: Parabolic (exponent = ");
    Serial.print(FAN_CURVE_EXPONENT);
    Serial.println(") for more aggressive cooling at high temps");
}

void loop() {
    // Check if it's time to calculate RPM
    if (tachometer.shouldCalculateRPM()) {
        tachometer.calculateRPM();
    }

    // Poll devices for temperature data
    deviceComm.pollDevices();
    
    // Fan speed is now updated immediately in temperature sensor callbacks
    // But we keep this call as a safety backup in case of any issues
    fanController.updateFanSpeed(tempSensor);
    
    // Check for direct commands from devices (outside of polling)
    deviceComm.checkIncomingData();
}
