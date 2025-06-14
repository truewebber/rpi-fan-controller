#ifndef DEVICE_COMMUNICATION_H
#define DEVICE_COMMUNICATION_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"
#include "temperature_sensor.h"

class DeviceCommunication {
private:
    SoftwareSerial device1;
    SoftwareSerial device2;
    SoftwareSerial device3;
    SoftwareSerial device4;
    SoftwareSerial* devices[NUM_DEVICES];
    
    String incomingData[NUM_DEVICES];
    bool deviceResponded[NUM_DEVICES];
    
    unsigned long lastPollTime;
    unsigned long commandSentTime;
    int currentPollingDevice;
    
    TemperatureSensor* tempSensor;

public:
    DeviceCommunication();
    
    // Initialize device communication
    void begin(TemperatureSensor* temperatureSensor);
    
    // Poll all devices in sequence
    void pollDevices();
    
    // Process response from specific device
    void processSerialResponse(int deviceId, const String& response);
    
    // Check for incoming data from all devices (outside of polling)
    void checkIncomingData();
    
    // Get device by ID
    SoftwareSerial* getDevice(int deviceId);
};

#endif // DEVICE_COMMUNICATION_H
