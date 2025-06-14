#include "device_communication.h"

DeviceCommunication::DeviceCommunication() 
    : device1(DEVICE1_RX_PIN, DEVICE1_TX_PIN),
      device2(DEVICE2_RX_PIN, DEVICE2_TX_PIN),
      device3(DEVICE3_RX_PIN, DEVICE3_TX_PIN),
      device4(DEVICE4_RX_PIN, DEVICE4_TX_PIN),
      lastPollTime(0),
      commandSentTime(0),
      currentPollingDevice(-1),
      tempSensor(nullptr) {
    
    devices[0] = &device1;
    devices[1] = &device2;
    devices[2] = &device3;
    devices[3] = &device4;
    
    for (int i = 0; i < NUM_DEVICES; i++) {
        incomingData[i] = "";
        deviceResponded[i] = false;
    }
}

void DeviceCommunication::begin(TemperatureSensor* temperatureSensor) {
    tempSensor = temperatureSensor;
    
    // Initialize SoftwareSerial ports
    device1.begin(BAUD_RATE);
    device2.begin(BAUD_RATE);
    device3.begin(BAUD_RATE);
    device4.begin(BAUD_RATE);
    
    // Stop listening on all ports initially
    device1.stopListening();
    device2.stopListening();
    device3.stopListening();
    device4.stopListening();
    
    // Debug: Print SoftwareSerial initialization
    Serial.println("SoftwareSerial initialization:");
    for(int i = 0; i < NUM_DEVICES; i++) {
        Serial.print("Device ");
        Serial.print(i + 1);
        Serial.print(": listening=");
        Serial.print(devices[i]->isListening());
        Serial.print(" baud=");
        Serial.println(BAUD_RATE);
    }
    
    Serial.println("Device communication initialized");
    Serial.println("Ready to communicate with 4 devices via SoftwareSerial");
    Serial.println("Polling for temperature data in format CPU:xx.x|NVME:xx.x");
}

void DeviceCommunication::pollDevices() {
    unsigned long currentMillis = millis();
    
    // If we're not currently polling and it's time to poll
    if (currentPollingDevice == -1 && currentMillis - lastPollTime >= POLL_INTERVAL) {
        currentPollingDevice = 0;
        lastPollTime = currentMillis;
        Serial.println("Starting device polling sequence");
    }
    
    // If we're in the middle of polling
    if (currentPollingDevice >= 0) {
        // If we haven't sent a command to the current device yet
        if (!deviceResponded[currentPollingDevice] && commandSentTime == 0) {
            // Stop listening on all ports
            for (int i = 0; i < NUM_DEVICES; i++) {
                devices[i]->stopListening();
            }
            
            // Start listening on current device
            devices[currentPollingDevice]->listen();
            
            // Give some time for the port to stabilize after switching
            delay(PORT_SWITCH_DELAY);
            
            // Clear any stale data in the buffer
            while (devices[currentPollingDevice]->available()) {
                devices[currentPollingDevice]->read();
            }
            
            // Send poll command to current device
            devices[currentPollingDevice]->println("POLL");
            
            // Ensure data is sent before continuing
            devices[currentPollingDevice]->flush();
            
            commandSentTime = currentMillis;
            Serial.print("Polling device ");
            Serial.print(currentPollingDevice + 1);
            Serial.println(" (sent: POLL)");
        }

        // Check if the current device has data available
        if (devices[currentPollingDevice]->available()) {
            char c = devices[currentPollingDevice]->read();
            if (c == '\n') {
                // Process the complete response
                processSerialResponse(currentPollingDevice, incomingData[currentPollingDevice]);
                incomingData[currentPollingDevice] = "";
                deviceResponded[currentPollingDevice] = true;
            } else if (c != '\r') {
                incomingData[currentPollingDevice] += c;
            }
        }

        // Check for timeout or if device responded
        if (deviceResponded[currentPollingDevice] || 
            (commandSentTime > 0 && currentMillis - commandSentTime >= RESPONSE_TIMEOUT)) {

            if (!deviceResponded[currentPollingDevice]) {
                Serial.print("Device ");
                Serial.print(currentPollingDevice + 1);
                Serial.println(" did not respond");
                
                // Handle missed poll
                if (tempSensor) {
                    tempSensor->handleMissedPoll(currentPollingDevice);
                }
            }

            // Move to next device
            deviceResponded[currentPollingDevice] = false;
            commandSentTime = 0;
            currentPollingDevice++;
            
            // If we've polled all devices, reset
            if (currentPollingDevice >= NUM_DEVICES) {
                currentPollingDevice = -1;
                Serial.println("Completed polling all devices");
                
                // After polling all devices, print a summary of temperatures
                if (tempSensor) {
                    tempSensor->printTemperatureSummary();
                }
            }
        }
    }
}

void DeviceCommunication::processSerialResponse(int deviceId, const String& response) {
    Serial.print("Device ");
    Serial.print(deviceId + 1);
    Serial.print(" sent: ");
    Serial.println(response);
    
    // Clean the response by removing any whitespace and line endings
    String cleanResponse = response;
    cleanResponse.trim();
    
    // CPU:xx.x|NVME:xx.x (temperature data)
    if (cleanResponse.startsWith("CPU:") && cleanResponse.indexOf("|NVME:") != -1) {
        // This is temperature data
        if (tempSensor && tempSensor->parseTemperatureData(deviceId, cleanResponse)) {
            // Reset missed polls counter on successful data reception
            tempSensor->resetMissedPolls(deviceId);
        }
    } else {
        Serial.print("Got unknown response: ");
        Serial.println(cleanResponse);
    }
}

void DeviceCommunication::checkIncomingData() {
    // Check for direct commands from devices (outside of polling)
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (devices[i]->available()) {
            char c = devices[i]->read();
            if (c == '\n') {
                processSerialResponse(i, incomingData[i]);
                incomingData[i] = "";
            } else if (c != '\r') {
                incomingData[i] += c;
            }
        }
    }
}

SoftwareSerial* DeviceCommunication::getDevice(int deviceId) {
    if (deviceId >= 0 && deviceId < NUM_DEVICES) {
        return devices[deviceId];
    }
    return nullptr;
}
