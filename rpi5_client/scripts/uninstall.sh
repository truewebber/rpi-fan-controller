#!/bin/bash
#
# Uninstallation script for Raspberry Pi Fan Temperature Daemon
#

# Exit on error
set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Please run with sudo: sudo ./scripts/uninstall.sh"
    exit 1
fi

echo "Uninstalling Raspberry Pi Fan Temperature Daemon..."

# Define paths
BINARY_PATH="/usr/local/bin/fan_temp_daemon"
SERVICE_FILE="/etc/systemd/system/fan-temp-daemon.service"
CONFIG_DIR="/etc/fan-temp-daemon"

# Stop and disable the service
echo "Stopping and disabling service..."
if systemctl is-active --quiet fan-temp-daemon.service; then
    systemctl stop fan-temp-daemon.service
    echo "Service stopped."
else
    echo "Service is not running."
fi

if systemctl is-enabled --quiet fan-temp-daemon.service 2>/dev/null; then
    systemctl disable fan-temp-daemon.service
    echo "Service disabled."
else
    echo "Service is not enabled."
fi

# Remove the binary
echo "Removing binary..."
if [ -f "$BINARY_PATH" ]; then
    rm -f "$BINARY_PATH"
    echo "Binary removed."
else
    echo "Binary not found at $BINARY_PATH."
fi

# Remove the service file
echo "Removing service file..."
if [ -f "$SERVICE_FILE" ]; then
    rm -f "$SERVICE_FILE"
    echo "Service file removed."
else
    echo "Service file not found at $SERVICE_FILE."
fi

# Remove the configuration directory
echo "Removing configuration directory..."
if [ -d "$CONFIG_DIR" ]; then
    rm -rf "$CONFIG_DIR"
    echo "Configuration directory removed."
else
    echo "Configuration directory not found at $CONFIG_DIR."
fi

# Reload systemd
echo "Reloading systemd daemon..."
systemctl daemon-reload
echo "Systemd daemon reloaded."

echo ""
echo "Uninstallation complete!"
echo "The Raspberry Pi Fan Temperature Daemon has been completely removed from your system." 