#!/bin/bash
#
# UART Fix Script for Raspberry Pi Fan Controller
# Automatically applies common fixes for UART communication issues
#

set -e

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

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_error "This script must be run as root"
    echo "Please run with sudo: sudo ./scripts/fix_uart.sh"
    exit 1
fi

print_header "Raspberry Pi UART Fix Tool"
print_status "This script will apply common fixes for UART communication issues"

# Backup config.txt
print_header "Creating Backup"
if [ -f "/boot/config.txt" ]; then
    cp /boot/config.txt "/boot/config.txt.backup.$(date +%Y%m%d_%H%M%S)"
    print_success "Backed up /boot/config.txt"
else
    print_error "/boot/config.txt not found"
    exit 1
fi

# Fix 1: Enable UART in /boot/config.txt
print_header "Fix 1: Enable UART"
if grep -q "enable_uart=1" /boot/config.txt; then
    print_success "UART already enabled in /boot/config.txt"
else
    echo "enable_uart=1" >> /boot/config.txt
    print_success "Added enable_uart=1 to /boot/config.txt"
fi

# Fix 2: Disable Bluetooth to free up UART
print_header "Fix 2: Disable Bluetooth"
if grep -q "dtoverlay=disable-bt" /boot/config.txt; then
    print_success "Bluetooth already disabled in /boot/config.txt"
else
    echo "dtoverlay=disable-bt" >> /boot/config.txt
    print_success "Added dtoverlay=disable-bt to /boot/config.txt"
fi

# Fix 3: Set core frequency to stabilize UART
print_header "Fix 3: Stabilize UART Clock"
if grep -q "core_freq=250" /boot/config.txt; then
    print_success "Core frequency already set"
else
    echo "core_freq=250" >> /boot/config.txt
    print_success "Added core_freq=250 to /boot/config.txt for stable UART"
fi

# Fix 4: Disable conflicting services
print_header "Fix 4: Disable Conflicting Services"
services_to_disable=("bluetooth" "hciuart")

for service in "${services_to_disable[@]}"; do
    if systemctl is-enabled "$service" &>/dev/null; then
        systemctl disable "$service"
        print_success "Disabled $service service"
    else
        print_status "$service service already disabled or not found"
    fi
done

# Fix 5: Stop getty on serial console
print_header "Fix 5: Disable Serial Console"
serial_services=("serial-getty@ttyAMA0" "serial-getty@serial0")

for service in "${serial_services[@]}"; do
    if systemctl is-active "$service" &>/dev/null; then
        systemctl stop "$service"
        systemctl disable "$service"
        print_success "Stopped and disabled $service"
    else
        print_status "$service already inactive"
    fi
done

# Fix 6: Update cmdline.txt to remove console
print_header "Fix 6: Remove Console from Kernel Command Line"
if [ -f "/boot/cmdline.txt" ]; then
    cp /boot/cmdline.txt "/boot/cmdline.txt.backup.$(date +%Y%m%d_%H%M%S)"
    print_success "Backed up /boot/cmdline.txt"
    
    # Remove console=serial0 and console=ttyAMA0 from cmdline.txt
    sed -i 's/console=serial0,[0-9]\+ //g' /boot/cmdline.txt
    sed -i 's/console=ttyAMA0,[0-9]\+ //g' /boot/cmdline.txt
    sed -i 's/console=serial0 //g' /boot/cmdline.txt
    sed -i 's/console=ttyAMA0 //g' /boot/cmdline.txt
    print_success "Removed serial console from kernel command line"
else
    print_warning "/boot/cmdline.txt not found"
fi

# Fix 7: Set proper permissions
print_header "Fix 7: Set Permissions"
for device in /dev/ttyAMA0 /dev/serial0; do
    if [ -e "$device" ]; then
        chmod 666 "$device"
        print_success "Set permissions for $device"
    fi
done

# Fix 8: Add user to dialout group
print_header "Fix 8: User Permissions"
if getent group dialout > /dev/null; then
    for user in pi $(logname 2>/dev/null || echo ""); do
        if id "$user" &>/dev/null; then
            usermod -a -G dialout "$user"
            print_success "Added $user to dialout group"
        fi
    done
else
    print_warning "dialout group not found"
fi

# Show final configuration
print_header "Final Configuration"
echo "Updated /boot/config.txt UART settings:"
grep -E "(enable_uart|dtoverlay.*bt|core_freq)" /boot/config.txt

print_header "Summary of Changes"
echo "✓ Enabled UART in /boot/config.txt"
echo "✓ Disabled Bluetooth to free up UART"
echo "✓ Stabilized UART clock frequency"
echo "✓ Disabled conflicting services"
echo "✓ Removed serial console"
echo "✓ Set proper permissions"

print_header "IMPORTANT: Reboot Required"
print_warning "You MUST reboot for these changes to take effect!"
echo ""
echo "After reboot:"
echo "1. Run the diagnostic script: sudo ./scripts/diagnose_uart.sh"
echo "2. Test the daemon: sudo systemctl restart fan-temp-daemon.service"
echo "3. Check logs: journalctl -u fan-temp-daemon.service -f"
echo ""
echo "Reboot now? (y/N)"
read -r response
if [[ "$response" =~ ^[Yy]$ ]]; then
    print_status "Rebooting in 5 seconds..."
    sleep 5
    reboot
else
    print_warning "Don't forget to reboot manually!"
fi 