#!/bin/bash

set -e

echo "Building fan_temp_daemon for Raspberry Pi 5 using Docker..."

# Build Docker image and compile the daemon
docker build --platform linux/arm64 --progress=plain -t rpi5-fan-daemon-builder .

# Create a temporary container to extract the binary
CONTAINER_ID=$(docker create --platform linux/arm64 rpi5-fan-daemon-builder)

# Copy the compiled .deb package from the container
docker cp $CONTAINER_ID:/build/rpi5-fan-temp-daemon-1.0.0.deb ./rpi5-fan-temp-daemon-1.0.0.deb

# Clean up the temporary container
docker rm $CONTAINER_ID

echo "Build complete!"
echo "Debian package: ./rpi5-fan-temp-daemon-1.0.0.deb"
echo "Transfer files to your Raspberry Pi 5"
