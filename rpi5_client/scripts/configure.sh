#!/bin/bash
#
# Configuration script for Raspberry Pi Fan Temperature Daemon
#

# Exit on error
set -e

echo "Raspberry Pi Fan Temperature Daemon Configuration"
echo "================================================="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Please run with sudo: sudo ./scripts/configure.sh"
    exit 1
fi

# Configuration directory and file
CONFIG_DIR="/etc/fan-temp-daemon"
CONFIG_FILE="$CONFIG_DIR/config"

# Create config directory
mkdir -p "$CONFIG_DIR"

# Set default values for most parameters
FAN_TEMP_SERIAL_PORT="/dev/serial0"
FAN_TEMP_BAUD_RATE="115200"
FAN_TEMP_READ_TIMEOUT="1"
FAN_TEMP_LOG_TO_SYSLOG="1"
FAN_TEMP_FOREGROUND="0"
FAN_TEMP_VERBOSE="0"

# Auto-detect CPU temperature command
echo "Auto-detecting CPU temperature command..."
CPU_CMD=""

# Try vcgencmd (standard on Raspberry Pi)
if command -v vcgencmd &> /dev/null; then
    if vcgencmd measure_temp &> /dev/null; then
        CPU_CMD="/usr/bin/vcgencmd measure_temp"
        echo "Found CPU temperature command: $CPU_CMD"
    fi
fi

# If vcgencmd not found, try thermal_zone
if [ -z "$CPU_CMD" ] && [ -d "/sys/class/thermal/thermal_zone0" ]; then
    if [ -f "/sys/class/thermal/thermal_zone0/temp" ]; then
        CPU_CMD="cat /sys/class/thermal/thermal_zone0/temp"
        echo "Found CPU temperature command: $CPU_CMD"
    fi
fi

# If still not found, use a fallback
if [ -z "$CPU_CMD" ]; then
    CPU_CMD="/usr/bin/vcgencmd measure_temp"
    echo "Warning: Could not auto-detect CPU temperature command"
    echo "Using default: $CPU_CMD"
fi

# Test CPU temperature command
echo "Testing CPU temperature command..."
CPU_TEMP_OUTPUT=$(eval $CPU_CMD 2>&1)
if [ $? -ne 0 ]; then
    echo "Warning: CPU temperature command failed: $CPU_TEMP_OUTPUT"
    echo "You may need to manually edit the configuration file after installation."
else
    echo "CPU temperature command test successful: $CPU_TEMP_OUTPUT"
fi

# Auto-detect NVME temperature command
echo "Auto-detecting NVME temperature command..."
NVME_CMD=""
NVME_DEVICE=""

# Check if smartmontools is installed
if ! command -v smartctl &> /dev/null; then
    echo "Installing smartmontools for NVME temperature monitoring..."
    apt-get update
    apt-get install -y smartmontools
fi

# Find NVME devices
for device in /dev/nvme?; do
    if [ -e "$device" ]; then
        if smartctl -A "$device" &> /dev/null; then
            NVME_DEVICE="$device"
            NVME_CMD="smartctl -A $NVME_DEVICE | grep Temperature"
            echo "Found NVME device: $NVME_DEVICE"
            break
        fi
    fi
done

# If no NVME device found, use a fallback
if [ -z "$NVME_CMD" ]; then
    NVME_CMD="smartctl -A /dev/nvme0 | grep Temperature"
    echo "Warning: Could not auto-detect NVME device"
    echo "Using default: $NVME_CMD"
fi

# Test NVME temperature command
echo "Testing NVME temperature command..."
NVME_TEMP_OUTPUT=$(eval $NVME_CMD 2>&1)
if [ $? -ne 0 ]; then
    echo "Warning: NVME temperature command failed: $NVME_TEMP_OUTPUT"
    echo "You may need to manually edit the configuration file after installation."
else
    echo "NVME temperature command test successful: $NVME_TEMP_OUTPUT"
fi

# Set the final command variables
FAN_TEMP_CPU_CMD="$CPU_CMD"
FAN_TEMP_NVME_CMD="$NVME_CMD"

# Create or overwrite the configuration file
echo "Creating configuration file at $CONFIG_FILE..."
cat > "$CONFIG_FILE" << EOF
# Raspberry Pi Fan Temperature Daemon Configuration
# Generated on $(date)
# Auto-configured by setup script

# Serial port to use
FAN_TEMP_SERIAL_PORT=$FAN_TEMP_SERIAL_PORT

# Baud rate (9600, 19200, 38400, 57600, 115200)
FAN_TEMP_BAUD_RATE=$FAN_TEMP_BAUD_RATE

# Timeout in seconds for reading from serial port
FAN_TEMP_READ_TIMEOUT=$FAN_TEMP_READ_TIMEOUT

# Log to syslog (1) or stdout (0)
FAN_TEMP_LOG_TO_SYSLOG=$FAN_TEMP_LOG_TO_SYSLOG

# Command to get CPU temperature
FAN_TEMP_CPU_CMD=$FAN_TEMP_CPU_CMD

# Command to get NVME temperature
FAN_TEMP_NVME_CMD=$FAN_TEMP_NVME_CMD

# Run in foreground (1) or background (0)
FAN_TEMP_FOREGROUND=$FAN_TEMP_FOREGROUND

# Enable verbose logging (1) or not (0)
FAN_TEMP_VERBOSE=$FAN_TEMP_VERBOSE
EOF

echo "Configuration complete!"
echo "Configuration file created at: $CONFIG_FILE"

# Check if the service is installed and restart it
if [ -f "/etc/systemd/system/fan-temp-daemon.service" ]; then
    echo "Fan temperature daemon service is installed."
    echo "Restarting service to apply new configuration..."
    systemctl restart fan-temp-daemon.service
    echo "Service restarted successfully!"
else
    echo "Fan temperature daemon service is not yet installed."
    echo "To install the daemon, run: ./scripts/install.sh"
fi 