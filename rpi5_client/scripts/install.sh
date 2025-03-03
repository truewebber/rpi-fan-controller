#!/bin/bash
#
# Installation script for Raspberry Pi Fan Temperature Daemon
#

# Exit on error
set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Please run with sudo: sudo ./scripts/install.sh"
    exit 1
fi

echo "Installing Raspberry Pi Fan Temperature Daemon..."

# Define paths
SCRIPT_DIR="$(dirname "$0")"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY_PATH="$PROJECT_DIR/bin/fan_temp_daemon"
SERVICE_FILE="$PROJECT_DIR/scripts/fan-temp-daemon.service"
CONFIG_DIR="/etc/fan-temp-daemon"
CONFIG_FILE="$CONFIG_DIR/config"
INSTALL_BIN_DIR="/usr/local/bin"
INSTALL_SERVICE_DIR="/etc/systemd/system"

# Check if the binary exists
if [ ! -f "$BINARY_PATH" ]; then
    echo "Error: Binary not found at $BINARY_PATH"
    echo "Please run the build script first: ./scripts/build.sh"
    exit 1
fi

# Check if the service file exists
if [ ! -f "$SERVICE_FILE" ]; then
    echo "Error: Service file not found at $SERVICE_FILE"
    exit 1
fi

# Check if this is an update or a new installation
IS_UPDATE=0
if [ -f "$INSTALL_BIN_DIR/fan_temp_daemon" ] || [ -f "$INSTALL_SERVICE_DIR/fan-temp-daemon.service" ]; then
    IS_UPDATE=1
    echo "Detected existing installation. Performing update..."
fi

# Save current service state before making changes
SERVICE_WAS_ACTIVE=0
SERVICE_WAS_ENABLED=0

if [ "$IS_UPDATE" -eq 1 ]; then
    # Check if service is currently active
    if systemctl is-active --quiet fan-temp-daemon.service; then
        SERVICE_WAS_ACTIVE=1
        echo "Service is currently active."
    else
        echo "Service is not currently active."
    fi
    
    # Check if service is enabled
    if systemctl is-enabled --quiet fan-temp-daemon.service 2>/dev/null; then
        SERVICE_WAS_ENABLED=1
        echo "Service is currently enabled to start at boot."
    else
        echo "Service is not currently enabled to start at boot."
    fi
fi

# Create installation directories if they don't exist
mkdir -p "$INSTALL_BIN_DIR"
mkdir -p "$CONFIG_DIR"

# Copy binary to installation directory
echo "Installing daemon binary to $INSTALL_BIN_DIR..."
install -m 755 "$BINARY_PATH" "$INSTALL_BIN_DIR/"
echo "Binary installed successfully."

# Copy service file to systemd directory
echo "Installing systemd service file to $INSTALL_SERVICE_DIR..."
install -m 644 "$SERVICE_FILE" "$INSTALL_SERVICE_DIR/"
echo "Service file installed successfully."

# Check if configuration exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "No configuration file found. Please run the configuration script:"
    echo "  sudo ./scripts/configure.sh"
    echo "Installation will continue, but the service may not start correctly."
fi

# Reload systemd to recognize the new service
echo "Reloading systemd daemon..."
systemctl daemon-reload
echo "Systemd daemon reloaded."

# Handle service state based on previous state or new installation
if [ "$IS_UPDATE" -eq 1 ]; then
    # For updates, maintain previous enabled state
    if [ "$SERVICE_WAS_ENABLED" -eq 1 ]; then
        echo "Maintaining enabled state for service."
        systemctl enable fan-temp-daemon.service
    else
        echo "Maintaining disabled state for service."
    fi
    
    # For updates, restart if it was active, start if it wasn't
    if [ "$SERVICE_WAS_ACTIVE" -eq 1 ]; then
        echo "Restarting service with updated binary and configuration..."
        systemctl restart fan-temp-daemon.service
        echo "Service restarted successfully."
    else
        echo "Starting service..."
        systemctl start fan-temp-daemon.service
        echo "Service started successfully."
    fi
else
    # For new installations, enable and start the service
    echo "Enabling service to start at boot..."
    systemctl enable fan-temp-daemon.service
    echo "Service enabled."
    
    echo "Starting service..."
    systemctl start fan-temp-daemon.service
    echo "Service started successfully."
fi

# Display service status
echo "Service status:"
systemctl status fan-temp-daemon.service --no-pager

echo ""
echo "Installation complete!"
if [ "$IS_UPDATE" -eq 1 ]; then
    echo "The daemon has been updated and is running."
else
    echo "The daemon is now installed and running."
fi
echo ""
echo "Configuration file: $CONFIG_FILE"
echo "To modify the configuration, edit this file and restart the service:"
echo "  sudo systemctl restart fan-temp-daemon.service"
echo ""
echo "To check the status: sudo systemctl status fan-temp-daemon.service"
echo "To stop the service: sudo systemctl stop fan-temp-daemon.service"
echo "To disable autostart: sudo systemctl disable fan-temp-daemon.service"
echo "To view logs: sudo journalctl -u fan-temp-daemon.service" 