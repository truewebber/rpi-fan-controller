FROM --platform=linux/arm64 gcc:latest

# Install necessary packages
RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source files
COPY . .

# Build the daemon
RUN ./scripts/build.sh && ./scripts/build_deb.sh

RUN ls -lah

# Output will be in bin/fan_temp_daemon
