#!/bin/bash
#
# Build script for Raspberry Pi Fan Temperature Daemon
#

# Exit on error
set -e

echo "Building Raspberry Pi Fan Temperature Daemon..."

# Install build dependencies if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Installing build dependencies..."
    apt-get update
    apt-get install -y build-essential
else
    echo "Note: Running as non-root user. Skipping package installation."
    echo "If build fails, please install required build dependencies manually:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install -y build-essential"
fi

# Build the daemon
echo "Building the daemon..."
cd "$(dirname "$0")/.."
make

echo "Build complete!"
echo "The binary is located at: $(pwd)/bin/fan_temp_daemon"
echo ""
echo "To install the daemon, run:"
echo "  sudo ./scripts/install.sh" 