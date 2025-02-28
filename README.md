# Raspberry Pi 5 Fan Controller

A smart PWM fan controller for Raspberry Pi 5 clusters that automatically adjusts fan speed based on CPU and NVME temperatures from up to 4 connected devices.

## Overview

This project implements an Arduino-based PWM fan controller that communicates with up to 4 Raspberry Pi 5 devices via serial connections. The controller monitors CPU and NVME temperatures from each connected device and automatically adjusts the fan speed to maintain optimal cooling while minimizing noise.

### Key Features

- **Automatic Temperature-Based Fan Control**: Adjusts fan speed based on the highest CPU and NVME temperatures across all connected devices
- **Multi-Device Support**: Monitors up to 4 Raspberry Pi 5 devices simultaneously
- **Tachometer Reading**: Provides real-time fan RPM feedback
- **High-Speed Communication**: Uses 115200 baud rate for responsive temperature monitoring
- **Disconnection Detection**: Automatically detects when devices connect or disconnect
- **Detailed Logging**: Provides comprehensive status information via Serial Monitor

## Hardware Requirements

### Controller
- Arduino Pro Mini 16MHz 5V
- PC Fan with PWM control and tachometer output
- Jumper wires
- Optional: 10kΩ pull-up resistor for tachometer (internal pull-up is used in code)

### Raspberry Pi 5 Devices
- 1-4 Raspberry Pi 5 boards
- Serial connection cables (UART pins or USB-to-Serial adapters)

## Wiring

### Fan Controller
- **Fan PWM**: Connect to pin 9
- **Fan Tachometer**: Connect to pin 3
- **SoftwareSerial connections**:
  - Device 1: RX on pin 4, TX on pin 5
  - Device 2: RX on pin 6, TX on pin 7
  - Device 3: RX on pin 8, TX on pin 10
  - Device 4: RX on pin 11, TX on pin 12

### Raspberry Pi 5 Connection
- **RX**: Connect to TX of the corresponding device on the fan controller
- **TX**: Connect to RX of the corresponding device on the fan controller
- **GND**: Connect to GND of the fan controller

## Automatic Fan Control Logic

The system automatically controls the fan speed based on temperature readings:

1. **Temperature Thresholds**:
   - CPU: 45°C (minimum speed) to 75°C (maximum speed)
   - NVME: 50°C (minimum speed) to 80°C (maximum speed)

2. **Fan Speed Range**:
   - Minimum: PWM value 30 (approximately 12% speed)
   - Maximum: PWM value 255 (100% speed)

3. **Control Algorithm**:
   - The system tracks the highest CPU and NVME temperatures from all connected devices
   - Fan speed is calculated separately for CPU and NVME temperatures using linear interpolation
   - The higher of the two calculated speeds is used
   - If no devices are connected, the fan runs at minimum speed

## Communication Protocol

The fan controller and Raspberry Pi devices communicate using a simple text-based protocol:

### Polling
The fan controller periodically polls each device with:
- `POLL` - Request status from device

Each device responds with temperature data in the format:
- `CPU:xxx.xx|NVME:xxx.xx` - Where xxx.xx is the temperature in Celsius

## Setup Instructions

### Arduino Setup
1. Clone this repository
2. Open the project in PlatformIO
3. Connect your Arduino Pro Mini to your computer
4. Upload the code to your Arduino Pro Mini

### Raspberry Pi Setup
1. Connect the Raspberry Pi's UART pins (or USB-to-Serial adapter) to the corresponding RX/TX pins on the Arduino
2. Compile and run the C program on each Raspberry Pi (see example below)
3. Consider setting up the program to run automatically at boot

## Example C Program for Raspberry Pi

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <errno.h>

#define SERIAL_PORT "/dev/ttyS0"  // Use appropriate port
#define BAUD_RATE B115200

// Function prototypes
float get_cpu_temperature(void);
float get_nvme_temperature(void);
int setup_serial(const char *port);
int send_data(int fd, const char *data);
int read_data(int fd, char *buffer, size_t size);

int main(void) {
    int serial_fd;
    char buffer[256];
    char temp_data[64];
    
    // Open and configure serial port
    serial_fd = setup_serial(SERIAL_PORT);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial port\n");
        return 1;
    }
    
    printf("Temperature monitoring started. Press Ctrl+C to exit.\n");

    while (1) {
        // Check if there's a command from the fan controller
        int bytes_read = read_data(serial_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';  // Null-terminate the string
            
            // Remove newline character if present
            if (buffer[bytes_read-1] == '\n') {
                buffer[bytes_read-1] = '\0';
            }
            
            printf("Received command: %s\n", buffer);
            
            if (strcmp(buffer, "POLL") == 0) {
                // Get current temperatures
                float cpu_temp = get_cpu_temperature();
                float nvme_temp = get_nvme_temperature();
                
                // Format temperature data
                snprintf(temp_data, sizeof(temp_data), "CPU:%.2f|NVME:%.2f\n", cpu_temp, nvme_temp);
                
                // Send temperature data
                send_data(serial_fd, temp_data);
                printf("Sent: %s", temp_data);
            }
        }
        
        // Small delay to prevent CPU hogging
        usleep(50000);  // 50ms
    }
    
    close(serial_fd);
    return 0;
}

// Configure and open serial port
int setup_serial(const char *port) {
    int fd;
    struct termios tty;
    
    // Open serial port
    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Error opening serial port");
        return -1;
    }
    
    // Get current settings
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error from tcgetattr");
        close(fd);
        return -1;
    }
    
    // Set baud rate
    cfsetospeed(&tty, BAUD_RATE);
    cfsetispeed(&tty, BAUD_RATE);
    
    // 8-bit chars, no parity, 1 stop bit
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;
    
    // No flow control
    tty.c_cflag &= ~(CRTSCTS);
    tty.c_cflag |= CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines
    
    // Set terminal attributes
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
        close(fd);
        return -1;
    }
    
    return fd;
}

// Send data to serial port
int send_data(int fd, const char *data) {
    return write(fd, data, strlen(data));
}

// Read data from serial port (non-blocking)
int read_data(int fd, char *buffer, size_t size) {
    fd_set rdset;
    struct timeval timeout;
    
    // Set up select() for non-blocking read
    FD_ZERO(&rdset);
    FD_SET(fd, &rdset);
    
    // Set timeout to 0 for non-blocking
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    // Check if data is available
    if (select(fd + 1, &rdset, NULL, NULL, &timeout) > 0) {
        return read(fd, buffer, size - 1);
    }
    
    return 0;  // No data available
}

// Get CPU temperature using vcgencmd
float get_cpu_temperature(void) {
    FILE *fp;
    char result[64];
    float temp = 0.0;
    
    // Execute vcgencmd to get CPU temperature
    fp = popen("/opt/vc/bin/vcgencmd measure_temp", "r");
    if (fp == NULL) {
        perror("Failed to run vcgencmd");
        return 50.0;  // Return default value on error
    }
    
    // Read the output
    if (fgets(result, sizeof(result), fp) != NULL) {
        // Parse the temperature value (format: temp=XX.X'C)
        char *temp_str = strstr(result, "temp=");
        if (temp_str != NULL) {
            sscanf(temp_str + 5, "%f", &temp);
        }
    }
    
    pclose(fp);
    return temp;
}

// Get NVME temperature using smartctl
float get_nvme_temperature(void) {
    FILE *fp;
    char line[256];
    float temp = 50.0;  // Default value
    
    // Execute smartctl to get NVME temperature
    fp = popen("smartctl -A /dev/nvme0 | grep Temperature", "r");
    if (fp == NULL) {
        perror("Failed to run smartctl");
        return temp;
    }
    
    // Read the output and parse temperature
    if (fgets(line, sizeof(line), fp) != NULL) {
        char *token = strtok(line, " ");
        int field_count = 0;
        
        // Temperature is typically in the 10th field
        while (token != NULL && field_count < 10) {
            token = strtok(NULL, " ");
            field_count++;
            
            if (field_count == 9 && token != NULL) {
                temp = atof(token);
                break;
            }
        }
    }
    
    pclose(fp);
    return temp;
}

### Compiling the C Program

To compile the program on your Raspberry Pi:

```bash
gcc -o temp_monitor temp_monitor.c -Wall
```

To run the program:

```bash
sudo ./temp_monitor
```

Note: You may need to install the `smartmontools` package to use the `smartctl` command:

```bash
sudo apt-get install smartmontools
```

### Setting Up Autostart

To make the program run automatically at boot, you can add it to `/etc/rc.local`:

```bash
sudo nano /etc/rc.local
```

Add this line before the `exit 0` line:

```bash
/path/to/temp_monitor &
```

## Customizing Temperature Thresholds

You can customize the temperature thresholds and fan speed range by modifying these constants in the code:

```cpp
// CPU temperature thresholds (in °C)
const float CPU_TEMP_MIN = 45.0;   // Below this, fan at minimum speed
const float CPU_TEMP_MAX = 75.0;   // Above this, fan at maximum speed

// NVME temperature thresholds (in °C)
const float NVME_TEMP_MIN = 50.0;  // Below this, fan at minimum speed
const float NVME_TEMP_MAX = 80.0;  // Above this, fan at maximum speed

// Fan speed settings
const int FAN_SPEED_MIN = 30;      // Minimum PWM value (0-255)
const int FAN_SPEED_MAX = 255;     // Maximum PWM value (0-255)
```

## Troubleshooting

- **No communication**: Check wiring, ensure GND is connected between devices
- **Garbled messages**: Verify baud rate is set to 115200 on all devices
- **Missing responses**: Check that all messages end with a newline character
- **Erratic RPM readings**: Ensure proper pull-up resistor on tachometer input
- **Temperature parsing issues**: Verify the format is exactly `CPU:xx.x|NVME:xx.x` with no spaces
- **SoftwareSerial reliability**: If experiencing issues, try shorter wires or reduce the baud rate

## License

This project is released under the MIT License. See the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
