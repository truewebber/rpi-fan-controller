#include <Arduino.h>
#include <stdio.h>
#include <math.h>
#include <SoftwareSerial.h>

// Forward declarations
void calculateRPM();
void tachISR();
void processSerialResponse(int deviceId, String command);
void pollDevices();
void parseTemperatureData(int deviceId, String data);
void updateFanSpeed();
void printTemperatureSummary();

// --- Serial Communication Speed ---
// const long BAUD_RATE = 57600;
const long BAUD_RATE = 38400;

// --- Pin Definitions for Arduino Pro Mini ---
const int fanPWMPin = 9;    // PWM output for fan control (pin 9 supports PWM)
const int tachPin   = 3;    // Tachometer input from the fan (pin 3 supports interrupts)

// --- SoftwareSerial Pin Definitions ---
// Note: Arduino Pro Mini has limited pins, so we're using pins that don't conflict with other functions
// Device 1 - RX: 4, TX: 5
// Device 2 - RX: 6, TX: 7
// Device 3 - RX: 8, TX: 10
// Device 4 - RX: 11, TX: 12
SoftwareSerial device1(4, 5);   // RX, TX
SoftwareSerial device2(6, 7);   // RX, TX
SoftwareSerial device3(8, 10);  // RX, TX
SoftwareSerial device4(11, 12); // RX, TX

// Array of SoftwareSerial pointers for easier iteration
SoftwareSerial* devices[] = {&device1, &device2, &device3, &device4};
const int NUM_DEVICES = 4;

// --- Communication Variables ---
const unsigned long pollInterval = 1000;  // Poll devices every 1 second
unsigned long lastPollTime = 0;
String incomingData[4] = {"", "", "", ""};
boolean deviceResponded[4] = {false, false, false, false};
const unsigned long responseTimeout = 200; // Wait 200ms for response
unsigned long commandSentTime = 0;
int currentPollingDevice = -1;  // -1 means not currently polling

// --- Temperature Variables ---
float cpuTemps[4] = {0.0, 0.0, 0.0, 0.0};
float nvmeTemps[4] = {0.0, 0.0, 0.0, 0.0};
boolean deviceConnected[4] = {false, false, false, false};
unsigned long lastTempUpdateTime[4] = {0, 0, 0, 0};
int missedPolls[4] = {0, 0, 0, 0}; // Counter for missed polls per device
const int maxMissedPolls = 10; // Consider device disconnected after 10 missed polls

// --- Temperature Thresholds for Fan Control ---
// CPU temperature thresholds (in °C)
const float CPU_TEMP_MIN = 40.0;   // Below this, fan at minimum speed
const float CPU_TEMP_MAX = 60.0;   // Above this, fan at maximum speed

// NVME temperature thresholds (in °C)
const float NVME_TEMP_MIN = 40.0;  // Below this, fan at minimum speed
const float NVME_TEMP_MAX = 65.0;  // Above this, fan at maximum speed

// Fan speed settings
const int FAN_SPEED_MIN = 30;      // Minimum PWM value (0-255)
const int FAN_SPEED_MAX = 255;     // Maximum PWM value (0-255)
const float FAN_CURVE_EXPONENT = 2.5;  // Parabolic curve exponent (>1 for aggressive cooling at high temps)

// --- Tachometer Variables ---
volatile unsigned int tachCount = 0;  // Incremented in the tachometer ISR
volatile unsigned int rpm = 0;        // Calculated RPM value

// --- Timer Variables for RPM Calculation ---
unsigned long lastRpmCalcTime = 0;
const unsigned long rpmCalcInterval = 1000; // Calculate RPM every 1000ms

// --- PWM Fan Speed Variable ---
int currentPwmValue = FAN_SPEED_MIN;

// --- Tachometer Interrupt Service Routine ---
// Called on every falling edge detected on tachPin
void tachISR() {
  tachCount++;
}

// --- RPM Calculation Function ---
void calculateRPM() {
  noInterrupts();               // Safely copy and reset tachCount
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
}

// --- Parse temperature data from devices ---
void parseTemperatureData(int deviceId, String data) {
  // Expected format: CPU:xx.x|NVME:xx.x
  
  // Update last temperature update time
  lastTempUpdateTime[deviceId] = millis();
  deviceConnected[deviceId] = true;
  missedPolls[deviceId] = 0; // Reset missed polls counter on successful data reception
  
  // Find the position of the CPU: and |NVME: markers
  int cpuPos = data.indexOf("CPU:");
  int nvmePos = data.indexOf("|NVME:");
  
  if (cpuPos != -1 && nvmePos != -1) {
    // Extract CPU temperature
    String cpuTempStr = data.substring(cpuPos + 4, nvmePos);
    cpuTemps[deviceId] = cpuTempStr.toFloat();
    
    // Extract NVME temperature
    String nvmeTempStr = data.substring(nvmePos + 6);
    nvmeTemps[deviceId] = nvmeTempStr.toFloat();
    
    // Log the parsed temperatures
    Serial.print("Device ");
    Serial.print(deviceId + 1);
    Serial.print(" temperatures - CPU: ");
    Serial.print(cpuTemps[deviceId]);
    Serial.print("°C, NVME: ");
    Serial.print(nvmeTemps[deviceId]);
    Serial.println("°C");
    
    // Update fan speed based on new temperature data
    updateFanSpeed();
  }
}

// --- Update fan speed based on temperature readings ---
void updateFanSpeed() {
  float highestCpuTemp = 0.0;
  float highestNvmeTemp = 0.0;
  bool anyTemperatureData = false;
  
  // Find the highest temperatures among all devices (using last saved values)
  for (int i = 0; i < NUM_DEVICES; i++) {
    // Use saved temperature values even if device is currently disconnected
    if (cpuTemps[i] > 0.0 || nvmeTemps[i] > 0.0) {
      anyTemperatureData = true;
      if (cpuTemps[i] > highestCpuTemp) {
        highestCpuTemp = cpuTemps[i];
      }
      if (nvmeTemps[i] > highestNvmeTemp) {
        highestNvmeTemp = nvmeTemps[i];
      }
    }
  }
  
  // If no temperature data is available at all, set fan to minimum speed
  if (!anyTemperatureData) {
    currentPwmValue = FAN_SPEED_MIN;
    analogWrite(fanPWMPin, currentPwmValue);
    Serial.println("No temperature data available. Fan set to minimum speed.");
    return;
  }
  
  // Calculate fan speed based on CPU temperature
  int cpuFanSpeed;
  if (highestCpuTemp <= CPU_TEMP_MIN) {
    cpuFanSpeed = FAN_SPEED_MIN;
  } else if (highestCpuTemp >= CPU_TEMP_MAX) {
    cpuFanSpeed = FAN_SPEED_MAX;
  } else {
    // Parabolic curve for more aggressive cooling at higher temperatures
    float cpuTempRatio = (highestCpuTemp - CPU_TEMP_MIN) / (CPU_TEMP_MAX - CPU_TEMP_MIN);
    float cpuCurvedRatio = pow(cpuTempRatio, FAN_CURVE_EXPONENT);
    cpuFanSpeed = FAN_SPEED_MIN + cpuCurvedRatio * (FAN_SPEED_MAX - FAN_SPEED_MIN);
  }
  
  // Calculate fan speed based on NVME temperature
  int nvmeFanSpeed;
  if (highestNvmeTemp <= NVME_TEMP_MIN) {
    nvmeFanSpeed = FAN_SPEED_MIN;
  } else if (highestNvmeTemp >= NVME_TEMP_MAX) {
    nvmeFanSpeed = FAN_SPEED_MAX;
  } else {
    // Parabolic curve for more aggressive cooling at higher temperatures
    float nvmeTempRatio = (highestNvmeTemp - NVME_TEMP_MIN) / (NVME_TEMP_MAX - NVME_TEMP_MIN);
    float nvmeCurvedRatio = pow(nvmeTempRatio, FAN_CURVE_EXPONENT);
    nvmeFanSpeed = FAN_SPEED_MIN + nvmeCurvedRatio * (FAN_SPEED_MAX - FAN_SPEED_MIN);
  }
  
  // Use the higher of the two calculated fan speeds
  int newPwmValue = max(cpuFanSpeed, nvmeFanSpeed);
  
  // Only update if the value has changed
  if (newPwmValue != currentPwmValue) {
    currentPwmValue = newPwmValue;
    analogWrite(fanPWMPin, currentPwmValue);
    
    // Log fan speed change
    Serial.print("Fan speed updated - PWM: ");
    Serial.print(currentPwmValue);
    Serial.print(" | Based on CPU: ");
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
      if (deviceConnected[i]) {
        Serial.print("ON");
      } else if (cpuTemps[i] > 0.0 || nvmeTemps[i] > 0.0) {
        Serial.print("OFF(saved)");
      } else {
        Serial.print("OFF");
      }
    }
    Serial.println();
  }
}

// --- Process commands from devices ---
void processSerialResponse(int deviceId, String response) {
  Serial.print("Device ");
  Serial.print(deviceId + 1);
  Serial.print(" sent: ");
  Serial.println(response);
  
  // Clean the response by removing any whitespace and line endings
  response.trim();
  
  // CPU:xx.x|NVME:xx.x (temperature data)
  if (response.startsWith("CPU:") && response.indexOf("|NVME:") != -1) {
    // This is temperature data
    parseTemperatureData(deviceId, response);
  } else {
    Serial.print("Got unknown response: ");
    Serial.println(response);
  }
}

// --- Print a summary of all device temperatures ---
void printTemperatureSummary() {
  Serial.println("=== Temperature Summary ===");
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (deviceConnected[i]) {
      Serial.print("Device ");
      Serial.print(i + 1);
      Serial.print(": CPU=");
      Serial.print(cpuTemps[i]);
      Serial.print("°C, NVME=");
      Serial.print(nvmeTemps[i]);
      Serial.print("°C, missed=");
      Serial.println(missedPolls[i]);
    } else {
      Serial.print("Device ");
      Serial.print(i + 1);
      Serial.print(": Not connected (missed=");
      Serial.print(missedPolls[i]);
      Serial.print(", last CPU=");
      Serial.print(cpuTemps[i]);
      Serial.print("°C, last NVME=");
      Serial.print(nvmeTemps[i]);
      Serial.println("°C)");
    }
  }
  Serial.print("Current Fan PWM: ");
  Serial.print(currentPwmValue);
  Serial.print(" (");
  Serial.print(map(currentPwmValue, 0, 255, 0, 100));
  Serial.println("%)");
  Serial.println("=========================");
}

// --- Poll each device in sequence ---
void pollDevices() {
  unsigned long currentMillis = millis();
  
  // Check for device timeouts - removed old timeout logic
  // Now using missed polls counter instead of time-based timeout
  
  // If we're not currently polling and it's time to poll
  if (currentPollingDevice == -1 && currentMillis - lastPollTime >= pollInterval) {
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
      delay(10);  // 10ms delay for port switching
      
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
        (commandSentTime > 0 && currentMillis - commandSentTime >= responseTimeout)) {

      if (!deviceResponded[currentPollingDevice]) {
        Serial.print("Device ");
        Serial.print(currentPollingDevice + 1);
        Serial.println(" did not respond");
        
        // Increment missed polls counter
        missedPolls[currentPollingDevice]++;
        Serial.print("Device ");
        Serial.print(currentPollingDevice + 1);
        Serial.print(" missed polls: ");
        Serial.println(missedPolls[currentPollingDevice]);
        
        // Check if device should be considered disconnected
        if (missedPolls[currentPollingDevice] >= maxMissedPolls && deviceConnected[currentPollingDevice]) {
          deviceConnected[currentPollingDevice] = false;
          Serial.print("Device ");
          Serial.print(currentPollingDevice + 1);
          Serial.println(" disconnected (too many missed polls)");
          
          // Update fan speed when a device disconnects
          updateFanSpeed();
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
        printTemperatureSummary();
      }
    }
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  
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
  
  // Initialize pins
  pinMode(fanPWMPin, OUTPUT);
  pinMode(tachPin, INPUT_PULLUP);  // Enable internal pull-up resistor for tach input

  // Attach the tachometer interrupt (FALLING edge triggers tachISR)
  attachInterrupt(digitalPinToInterrupt(tachPin), tachISR, FALLING);

  // Initialize fan PWM to minimum speed
  currentPwmValue = FAN_SPEED_MIN;
  analogWrite(fanPWMPin, currentPwmValue);

  // Log system initialization
  Serial.println("System Initialized.");
  Serial.println("Ready to communicate with 4 devices via SoftwareSerial");
  Serial.println("Polling for temperature data in format CPU:xx.x|NVME:xx.x");
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
  unsigned long currentMillis = millis();
  unsigned long millisSinceLastRPMCalc = currentMillis - lastRpmCalcTime;
  
  if (millisSinceLastRPMCalc >= rpmCalcInterval) {
    calculateRPM();
    lastRpmCalcTime = currentMillis;
  }

  // Poll devices
  pollDevices();
  
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
