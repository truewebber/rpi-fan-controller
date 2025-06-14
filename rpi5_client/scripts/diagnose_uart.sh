#!/bin/bash
#
# UART Diagnostic Script for Raspberry Pi Fan Controller
# This script helps diagnose communication issues with Arduino
#

set -e

echo "=========================================="
echo "Raspberry Pi UART Diagnostic Tool"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

# 1. System Information
print_header "System Information"
print_status "Hostname: $(hostname)"
print_status "Raspberry Pi Model: $(cat /proc/device-tree/model 2>/dev/null || echo 'Unknown')"
print_status "Kernel: $(uname -r)"
print_status "Date: $(date)"

# 2. Check /boot/firmware/config.txt for UART settings
print_header "Boot Configuration (/boot/firmware/config.txt)"
if [ -f "/boot/firmware/config.txt" ]; then
    echo "Relevant UART settings:"
    grep -E "(enable_uart|dtoverlay.*uart)" /boot/firmware/config.txt || echo "No UART settings found"
    
    # Check if UART is enabled
    if grep -q "enable_uart=1" /boot/firmware/config.txt; then
        print_success "UART is enabled in /boot/firmware/config.txt"
    else
        print_error "UART is NOT enabled in /boot/firmware/config.txt"
        echo "Add this line to /boot/firmware/config.txt: enable_uart=1"
    fi
    
    # Check for Bluetooth disable
    if grep -q "dtoverlay=disable-bt" /boot/firmware/config.txt; then
        print_success "Bluetooth is disabled (good for UART)"
    else
        print_warning "Bluetooth is NOT disabled - this may interfere with UART"
        echo "Consider adding: dtoverlay=disable-bt"
    fi
else
    print_error "/boot/firmware/config.txt not found"
fi

# 3. Check available serial devices
print_header "Available Serial Devices"
echo "All tty devices:"
ls -la /dev/tty* | grep -E "(ttyAMA|ttyS|serial)" || echo "None found"

echo -e "\nSerial device details:"
for device in /dev/ttyAMA0 /dev/ttyAMA1 /dev/serial0 /dev/serial1; do
    if [ -e "$device" ]; then
        if [ -r "$device" ] && [ -w "$device" ]; then
            print_success "$device exists and is readable/writable"
        else
            print_warning "$device exists but has permission issues"
            ls -la "$device"
        fi
    else
        print_error "$device does not exist"
    fi
done

# 4. Check GPIO pin configuration
print_header "GPIO Pin Configuration"
if command -v gpio &> /dev/null; then
    echo "GPIO 14 (TX) and GPIO 15 (RX) status:"
    gpio readall | grep -E "(14|15)" || echo "Could not read GPIO status"
else
    print_warning "gpio command not available (install wiringpi)"
fi

# 5. Check systemd services that might interfere
print_header "Conflicting Services"
services_to_check=("bluetooth" "hciuart" "serial-getty@ttyAMA0" "serial-getty@serial0")

for service in "${services_to_check[@]}"; do
    status=$(systemctl is-active "$service" 2>/dev/null || echo "not-found")
    case $status in
        "active")
            print_error "$service is running (may conflict with UART)"
            echo "Consider disabling: sudo systemctl disable $service"
            ;;
        "inactive")
            print_success "$service is inactive"
            ;;
        "not-found")
            print_status "$service service not found"
            ;;
    esac
done

# 6. Test raw serial communication
print_header "Raw Serial Communication Test"
FAN_TEMP_SERIAL_PORT="/dev/ttyAMA0"

# Try to find the configured port
if [ -f "/etc/fan-temp-daemon/config" ]; then
    configured_port=$(grep "FAN_TEMP_SERIAL_PORT" /etc/fan-temp-daemon/config | cut -d'=' -f2)
    if [ -n "$configured_port" ]; then
        FAN_TEMP_SERIAL_PORT="$configured_port"
        print_status "Using configured port: $FAN_TEMP_SERIAL_PORT"
    fi
else
    print_status "Using default port: $FAN_TEMP_SERIAL_PORT"
fi

if [ -e "$FAN_TEMP_SERIAL_PORT" ]; then
    print_status "Testing communication on $FAN_TEMP_SERIAL_PORT"
    
    # Configure port for raw testing
    if command -v stty &> /dev/null; then
        print_status "Configuring port with stty..."
        stty -F "$FAN_TEMP_SERIAL_PORT" 38400 cs8 -cstopb -parenb raw -echo || print_error "Failed to configure port"
        
        # Try to read raw data
        print_status "Listening for data for 5 seconds..."
        echo "Raw data received:"
        timeout 5s hexdump -C "$FAN_TEMP_SERIAL_PORT" || print_status "No data received (timeout)"
    else
        print_error "stty command not available"
    fi
else
    print_error "$FAN_TEMP_SERIAL_PORT does not exist"
fi

# 7. Check current daemon status
print_header "Fan Temperature Daemon Status"
if systemctl is-active fan-temp-daemon.service &>/dev/null; then
    print_success "fan-temp-daemon.service is running"
    echo "Recent logs:"
    journalctl -u fan-temp-daemon.service --no-pager -n 10
else
    print_warning "fan-temp-daemon.service is not running"
fi

# 8. Hardware connection test
print_header "Hardware Connection Recommendations"
print_status "Arduino-to-Pi wiring should be:"
echo "  Arduino Pin 4  (RX) -> Pi GPIO 14 (TX) - Pin 8"
echo "  Arduino Pin 5  (TX) -> Pi GPIO 15 (RX) - Pin 10"
echo "  Arduino GND         -> Pi GND          - Pin 6"
echo ""
print_warning "IMPORTANT: Arduino is 5V, Pi is 3.3V!"
echo "  Use level shifter or voltage divider for safe connection"
echo "  TX from Arduino (5V) to Pi RX (3.3V) needs voltage reduction"
echo "  TX from Pi (3.3V) to Arduino RX (5V) usually works directly"

# 9. Suggested fixes
print_header "Suggested Fixes for Non-Working Devices"
echo "1. Check /boot/firmware/config.txt and add if missing:"
echo "   enable_uart=1"
echo "   dtoverlay=disable-bt"
echo ""
echo "2. Disable conflicting services:"
echo "   sudo systemctl disable bluetooth"
echo "   sudo systemctl disable hciuart"
echo "   sudo systemctl disable serial-getty@ttyAMA0"
echo ""
echo "3. Reboot after changes:"
echo "   sudo reboot"
echo ""
echo "4. Test with manual communication:"
echo "   echo 'POLL' > $FAN_TEMP_SERIAL_PORT"
echo "   cat $FAN_TEMP_SERIAL_PORT"
echo ""
echo "5. Check hardware connections and voltage levels"

print_header "Diagnostic Complete"
echo "Save this output and compare between working and non-working devices" 