#ifndef CONFIG_H
#define CONFIG_H

// --- Serial Communication Speed ---
const long BAUD_RATE = 38400;

// --- Pin Definitions for Arduino Pro Mini ---
const int FAN_PWM_PIN = 9;    // PWM output for fan control
const int TACH_PIN = 3;       // Tachometer input from the fan

// --- SoftwareSerial Pin Definitions ---
// Device 1 - RX: 4, TX: 5
// Device 2 - RX: 6, TX: 7
// Device 3 - RX: 8, TX: 10
// Device 4 - RX: 11, TX: 12
const int DEVICE1_RX_PIN = 4;
const int DEVICE1_TX_PIN = 5;
const int DEVICE2_RX_PIN = 6;
const int DEVICE2_TX_PIN = 7;
const int DEVICE3_RX_PIN = 8;
const int DEVICE3_TX_PIN = 10;
const int DEVICE4_RX_PIN = 11;
const int DEVICE4_TX_PIN = 12;

// --- Device Configuration ---
const int NUM_DEVICES = 4;
const unsigned long POLL_INTERVAL = 1000;        // Poll devices every 1 second
const unsigned long RESPONSE_TIMEOUT = 200;      // Wait 200ms for response
const int MAX_MISSED_POLLS = 10;                 // Consider device disconnected after 10 missed polls

// --- Temperature Thresholds for Fan Control ---
// CPU temperature thresholds (in °C)
const float CPU_TEMP_MIN = 40.0;   // Below this, fan at minimum speed
const float CPU_TEMP_MAX = 60.0;   // Above this, fan at maximum speed

// NVME temperature thresholds (in °C)
const float NVME_TEMP_MIN = 40.0;  // Below this, fan at minimum speed
const float NVME_TEMP_MAX = 65.0;  // Above this, fan at maximum speed

// --- Fan Speed Settings ---
const int FAN_SPEED_MIN = 30;                    // Minimum PWM value (0-255)
const int FAN_SPEED_MAX = 255;                   // Maximum PWM value (0-255)
const float FAN_CURVE_EXPONENT = 2.5;           // Parabolic curve exponent

// --- Timing Constants ---
const unsigned long RPM_CALC_INTERVAL = 1000;   // Calculate RPM every 1000ms
const int PORT_SWITCH_DELAY = 50;               // 50ms delay for port switching (increased for stability)

#endif // CONFIG_H
