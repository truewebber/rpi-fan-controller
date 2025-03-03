#!/bin/bash
#
# Script to build a Debian package for Raspberry Pi Fan Temperature Daemon
#

# Exit on error
set -e

echo "Building Debian package for Raspberry Pi Fan Temperature Daemon..."

# Define paths and package information
SCRIPT_DIR="$(dirname "$0")"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PACKAGE_NAME="rpi5-fan-temp-daemon"
PACKAGE_VERSION="1.0.0"
PACKAGE_ARCH="arm64"
PACKAGE_MAINTAINER="Your Name <your.email@example.com>"
PACKAGE_DESCRIPTION="Raspberry Pi 5 Fan Temperature Daemon"
PACKAGE_DEPENDS="systemd, smartmontools"

# Create temporary build directory
BUILD_DIR="$PROJECT_DIR/build/deb"
PACKAGE_DIR="$BUILD_DIR/$PACKAGE_NAME-$PACKAGE_VERSION"
DEBIAN_DIR="$PACKAGE_DIR/DEBIAN"
BIN_DIR="$PACKAGE_DIR/usr/local/bin"
SERVICE_DIR="$PACKAGE_DIR/etc/systemd/system"
CONFIG_DIR="$PACKAGE_DIR/etc/fan-temp-daemon"
DOC_DIR="$PACKAGE_DIR/usr/share/doc/$PACKAGE_NAME"

# Clean up previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$DEBIAN_DIR"
mkdir -p "$BIN_DIR"
mkdir -p "$SERVICE_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$DOC_DIR"

# Check if the binary exists
if [ ! -f "$PROJECT_DIR/bin/fan_temp_daemon" ]; then
    echo "Error: Binary not found at $PROJECT_DIR/bin/fan_temp_daemon"
    echo "Please run the build script first: ./scripts/build.sh"
    exit 1
fi

# Check if the service file exists
if [ ! -f "$PROJECT_DIR/scripts/fan-temp-daemon.service" ]; then
    echo "Error: Service file not found at $PROJECT_DIR/scripts/fan-temp-daemon.service"
    exit 1
fi

# Copy binary, service file, and README
cp "$PROJECT_DIR/bin/fan_temp_daemon" "$BIN_DIR/"
cp "$PROJECT_DIR/scripts/fan-temp-daemon.service" "$SERVICE_DIR/"
cp "$PROJECT_DIR/README.md" "$DOC_DIR/README.md"

# Create control file
cat > "$DEBIAN_DIR/control" << EOF
Package: $PACKAGE_NAME
Version: $PACKAGE_VERSION
Section: utils
Priority: optional
Architecture: $PACKAGE_ARCH
Depends: $PACKAGE_DEPENDS
Maintainer: $PACKAGE_MAINTAINER
Description: $PACKAGE_DESCRIPTION
 A daemon for Raspberry Pi 5 that monitors CPU and NVME temperatures
 and responds to polling requests from an Arduino-based fan controller.
 It runs in the background and listens for POLL commands on a serial port.
EOF

# Use configure.sh as preinst script
cp "$PROJECT_DIR/scripts/configure.sh" "$DEBIAN_DIR/preinst"

# Create postinst script
cat > "$DEBIAN_DIR/postinst" << EOF
#!/bin/bash
set -e

# Reload systemd to recognize the new service
systemctl daemon-reload

# Enable and start the service
systemctl enable fan-temp-daemon.service
systemctl start fan-temp-daemon.service

echo "Raspberry Pi Fan Temperature Daemon has been installed and started."
echo "Configuration file: /etc/fan-temp-daemon/config"
echo "To check status: systemctl status fan-temp-daemon.service"
exit 0
EOF

# Create prerm script
cat > "$DEBIAN_DIR/prerm" << EOF
#!/bin/bash
set -e

# Stop and disable the service
if systemctl is-active --quiet fan-temp-daemon.service; then
    systemctl stop fan-temp-daemon.service
fi

if systemctl is-enabled --quiet fan-temp-daemon.service 2>/dev/null; then
    systemctl disable fan-temp-daemon.service
fi

exit 0
EOF

# Create postrm script
cat > "$DEBIAN_DIR/postrm" << EOF
#!/bin/bash
set -e

# Remove configuration directory if this is a purge
if [ "\$1" = "purge" ]; then
    rm -rf /etc/fan-temp-daemon
fi

# Reload systemd
systemctl daemon-reload

exit 0
EOF

# Make scripts executable
chmod 755 "$DEBIAN_DIR/preinst"
chmod 755 "$DEBIAN_DIR/postinst"
chmod 755 "$DEBIAN_DIR/prerm"
chmod 755 "$DEBIAN_DIR/postrm"

# Set permissions
find "$PACKAGE_DIR" -type d -exec chmod 755 {} \;
find "$PACKAGE_DIR" -type f -exec chmod 644 {} \;
chmod 755 "$BIN_DIR/fan_temp_daemon"

# Build the package
echo "Building Debian package..."
dpkg-deb --build "$PACKAGE_DIR"

# Move the package to the project root
mv "$BUILD_DIR/$PACKAGE_NAME-$PACKAGE_VERSION.deb" "$PROJECT_DIR/$PACKAGE_NAME-$PACKAGE_VERSION.deb"

echo ""
echo "Debian package created successfully: $PROJECT_DIR/$PACKAGE_NAME-$PACKAGE_VERSION.deb"
echo ""
echo "To install the package:"
echo "  sudo dpkg -i $PACKAGE_NAME-$PACKAGE_VERSION.deb"
echo "  sudo apt-get install -f  # To resolve any dependencies"
echo ""
echo "To remove the package:"
echo "  sudo apt-get remove $PACKAGE_NAME"
echo ""
echo "To completely remove the package including configuration:"
echo "  sudo apt-get purge $PACKAGE_NAME" 