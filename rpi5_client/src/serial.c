/**
 * Serial communication module for Fan Temperature Daemon
 * Handles all serial port operations including buffered reading and synchronization
 */

#include "serial.h"
#include "logger.h"
#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/ioctl.h>

// Static buffer for command reading
static char g_read_buffer[512] = {0};
static int g_buffer_pos = 0;

/**
 * Reset the internal read buffer
 */
void serial_reset_read_buffer(void) {
    g_buffer_pos = 0;
    memset(g_read_buffer, 0, sizeof(g_read_buffer));
    if (g_config.verbose) {
        LOG_MESSAGE_DEBUG("Serial read buffer reset");
    }
}

/**
 * Clear serial port buffers
 */
void serial_clear_buffers(int fd) {
    if (fd >= 0) {
        tcflush(fd, TCIOFLUSH);  // Clear both input and output buffers
    }
}

/**
 * Check if serial connection is healthy
 */
int serial_check_health(int fd) {
    if (fd < 0) {
        return 0;
    }
    
    int status;
    if (ioctl(fd, TIOCMGET, &status) == -1) {
        return 0;  // Connection problem
    }
    return 1;  // Connection OK
}

/**
 * Aggressively clear serial port and recover synchronization
 */
void serial_recover_synchronization(int fd) {
    char discard_buffer[256];
    int attempts = 0;
    
    if (g_config.verbose) {
        LOG_MESSAGE_DEBUG("Starting serial synchronization recovery");
    }
    
    // Clear hardware buffers multiple times with longer delays
    for (int i = 0; i < 5; i++) {
        serial_clear_buffers(fd);
        utils_sleep_ms(200);  // 200ms between flushes
    }
    
    // Send a few dummy bytes to trigger any pending Arduino transmissions
    const char *sync_chars = "\n\n\n";
    write(fd, sync_chars, strlen(sync_chars));
    fsync(fd);  // Force data to be sent
    utils_sleep_ms(300);  // 300ms to let Arduino respond
    
    // Clear buffers again after sync attempt
    serial_clear_buffers(fd);
    utils_sleep_ms(100);  // 100ms
    
    // Read and discard any remaining data with timeout
    fd_set rdset;
    struct timeval timeout;
    
    while (attempts < 20) {  // Max 20 attempts
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms timeout
        
        int select_result = select(fd + 1, &rdset, NULL, NULL, &timeout);
        
        if (select_result > 0) {
            int bytes_discarded = read(fd, discard_buffer, sizeof(discard_buffer) - 1);
            if (bytes_discarded > 0) {
                discard_buffer[bytes_discarded] = '\0';
                if (g_config.verbose) {
                    // Clean up the discarded data for logging
                    char clean_discard[64];
                    size_t j = 0;
                    for (int i = 0; i < bytes_discarded && j < sizeof(clean_discard) - 3; i++) {
                        if (discard_buffer[i] == '\n') {
                            clean_discard[j++] = '\\';
                            clean_discard[j++] = 'n';
                        } else if (discard_buffer[i] == '\r') {
                            clean_discard[j++] = '\\';
                            clean_discard[j++] = 'r';
                        } else if (discard_buffer[i] >= 32 && discard_buffer[i] <= 126) {
                            clean_discard[j++] = discard_buffer[i];
                        } else {
                            clean_discard[j++] = '?';
                        }
                    }
                    clean_discard[j] = '\0';
                    LOG_MESSAGE_DEBUG("Discarded stale data: '%s' (%d bytes)", clean_discard, bytes_discarded);
                }
                attempts++;
            } else {
                break;  // No more data
            }
        } else {
            break;  // Timeout or error - no more data
        }
    }
    
    // Final aggressive cleanup sequence
    serial_clear_buffers(fd);
    utils_sleep_ms(100);  // 100ms
    serial_clear_buffers(fd);
    utils_sleep_ms(50);   // 50ms
    
    // Reset internal read buffer
    serial_reset_read_buffer();
    
    if (g_config.verbose) {
        LOG_MESSAGE_DEBUG("Serial synchronization recovery completed (%d cleanup attempts)", attempts);
    }
}

/**
 * Configure and open serial port
 */
int serial_setup(const char *port, speed_t baud_rate) {
    int fd;
    struct termios tty;
    
    if (port == NULL) {
        LOG_MESSAGE_ERR("Serial port is NULL");
        return -1;
    }
    
    // Open serial port with additional flags for reliability
    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) {
        LOG_MESSAGE_ERR("Error opening serial port %s: %s", port, strerror(errno));
        return -1;
    }
    
    // Remove non-blocking flag after opening (we'll use select() for timeouts)
    int flags = fcntl(fd, F_GETFL);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    
    // Clear any existing data in buffers
    serial_clear_buffers(fd);
    
    // Get current settings
    if (tcgetattr(fd, &tty) != 0) {
        LOG_MESSAGE_ERR("Error from tcgetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }
    
    // Set baud rate
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);
    
    // Configure for 8N1 (8 data bits, no parity, 1 stop bit)
    tty.c_cflag &= ~PARENB;        // Clear parity bit, disabling parity
    tty.c_cflag &= ~CSTOPB;        // Clear stop field, only one stop bit used
    tty.c_cflag &= ~CSIZE;         // Clear all bits that set the data size
    tty.c_cflag |= CS8;            // 8 bits per byte
    
    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control
    
    // Enable receiver and set local mode
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines
    
    // Disable software flow control
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    
    // Disable special input processing
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_iflag &= ~(INLCR | ICRNL | IUCLC | IMAXBEL);
    
    // Make raw (disable all output processing)
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~(OLCUC | ONLCR | OCRNL | ONOCR | ONLRET | OFILL | OFDEL);
    
    // Make raw (disable all local processing)
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
    tty.c_lflag &= ~(XCASE);
    
    // Set read behavior - non-blocking read with timeout
    tty.c_cc[VMIN] = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 2;  // 0.2 second timeout
    
    // Set terminal attributes immediately
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        LOG_MESSAGE_ERR("Error from tcsetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }
    
    // Verify settings were applied correctly
    struct termios verify_tty;
    if (tcgetattr(fd, &verify_tty) == 0) {
        if (verify_tty.c_cflag != tty.c_cflag || 
            verify_tty.c_iflag != tty.c_iflag ||
            verify_tty.c_oflag != tty.c_oflag ||
            verify_tty.c_lflag != tty.c_lflag) {
            LOG_MESSAGE_WARNING("Serial port settings may not have been applied correctly");
        }
    }
    
    // Additional hardware-specific optimizations for Raspberry Pi
    int buffer_size = 8192;  // 8KB buffers
    if (ioctl(fd, TIOCOUTQ, &buffer_size) == 0) {
        if (g_config.verbose) {
            LOG_MESSAGE_DEBUG("Set output buffer size to %d bytes", buffer_size);
        }
    }
    
    // Disable exclusive mode to prevent locking issues
    if (ioctl(fd, TIOCNXCL) == -1) {
        if (g_config.verbose) {
            LOG_MESSAGE_DEBUG("Could not disable exclusive mode: %s", strerror(errno));
        }
    }
    
    // Clear buffers again after configuration
    serial_clear_buffers(fd);
    
    // Perform synchronization recovery
    serial_recover_synchronization(fd);
    
    return fd;
}

/**
 * Send data to serial port
 */
int serial_send_data(int fd, const char *data) {
    if (fd < 0 || data == NULL) {
        return -1;
    }
    
    return write(fd, data, strlen(data));
}

/**
 * Read data from serial port (blocking with timeout)
 */
int serial_read_data(int fd, char *buffer, size_t size, int timeout_sec) {
    if (fd < 0 || buffer == NULL || size == 0) {
        return -1;
    }
    
    fd_set rdset;
    struct timeval timeout;
    
    // Set up select() for blocking read with timeout
    FD_ZERO(&rdset);
    FD_SET(fd, &rdset);
    
    // Set timeout
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    
    // Wait for data with timeout
    int select_result = select(fd + 1, &rdset, NULL, NULL, &timeout);
    
    if (select_result > 0) {
        // Data is available, read it
        return read(fd, buffer, size - 1);
    } else if (select_result == 0) {
        // Timeout occurred
        return 0;
    } else {
        // Error occurred
        LOG_MESSAGE_ERR("Select error: %s", strerror(errno));
        return -1;
    }
}

/**
 * Read complete command from serial port (buffered with timeout)
 * This function accumulates data and extracts commands ending with \r\n or \n
 */
int serial_read_complete_command(int fd, char *buffer, size_t size, int timeout_sec) {
    char temp_buf[64];
    
    if (fd < 0 || buffer == NULL || size == 0) {
        return -1;
    }
    
    fd_set rdset;
    struct timeval timeout;
    
    // Set up select() for blocking read with timeout
    FD_ZERO(&rdset);
    FD_SET(fd, &rdset);
    
    // Set timeout
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    
    // Wait for data with timeout
    int select_result = select(fd + 1, &rdset, NULL, NULL, &timeout);
    
    if (select_result > 0) {
        // Data is available, read it
        int bytes_read = read(fd, temp_buf, sizeof(temp_buf) - 1);
        
        if (bytes_read > 0) {
            temp_buf[bytes_read] = '\0';  // Null-terminate
            
            if (g_config.verbose) {
                // Log raw bytes in hex for debugging
                char hex_log[256];
                int hex_pos = 0;
                hex_pos += snprintf(hex_log + hex_pos, sizeof(hex_log) - hex_pos, "Raw hex: ");
                for (int i = 0; i < bytes_read && hex_pos < (int)sizeof(hex_log) - 4; i++) {
                    hex_pos += snprintf(hex_log + hex_pos, sizeof(hex_log) - hex_pos, "%02X ", (unsigned char)temp_buf[i]);
                }
                LOG_MESSAGE_DEBUG("%s", hex_log);
                LOG_MESSAGE_DEBUG("Raw data received: %d bytes", bytes_read);
            }
            
            // Add new data to our buffer, handling overflow
            for (int i = 0; i < bytes_read; i++) {
                if (g_buffer_pos < (int)(sizeof(g_read_buffer) - 1)) {
                    g_read_buffer[g_buffer_pos++] = temp_buf[i];
                } else {
                    // Buffer overflow - shift left and add new char
                    memmove(g_read_buffer, g_read_buffer + 1, sizeof(g_read_buffer) - 2);
                    g_read_buffer[sizeof(g_read_buffer) - 2] = temp_buf[i];
                    g_buffer_pos = sizeof(g_read_buffer) - 1;
                }
            }
            g_read_buffer[g_buffer_pos] = '\0';
            
            if (g_config.verbose) {
                // Create a clean version of buffer for logging
                char clean_log[128];
                int clean_pos = 0;
                for (int i = 0; i < g_buffer_pos && i < (int)sizeof(g_read_buffer) && clean_pos < 120; i++) {
                    if (g_read_buffer[i] == '\n') {
                        clean_log[clean_pos++] = '\\';
                        clean_log[clean_pos++] = 'n';
                    } else if (g_read_buffer[i] == '\r') {
                        clean_log[clean_pos++] = '\\';
                        clean_log[clean_pos++] = 'r';
                    } else if (g_read_buffer[i] >= 32 && g_read_buffer[i] <= 126) {
                        clean_log[clean_pos++] = g_read_buffer[i];
                    } else {
                        clean_log[clean_pos++] = '?';
                    }
                }
                clean_log[clean_pos] = '\0';
                LOG_MESSAGE_DEBUG("Buffer now: '%s' (%d chars)", clean_log, g_buffer_pos);
            }
            
            // Look for command ending with \r\n or \n
            char *newline_pos = NULL;
            int cmd_end = -1;
            
            // Search for \r\n first (Arduino format)
            for (int i = 0; i < g_buffer_pos - 1; i++) {
                if (g_read_buffer[i] == '\r' && g_read_buffer[i + 1] == '\n') {
                    newline_pos = &g_read_buffer[i];
                    cmd_end = i;
                    break;
                }
            }
            
            // If no \r\n found, look for standalone \n
            if (newline_pos == NULL) {
                for (int i = 0; i < g_buffer_pos; i++) {
                    if (g_read_buffer[i] == '\n') {
                        newline_pos = &g_read_buffer[i];
                        cmd_end = i;
                        break;
                    }
                }
            }
            
            if (newline_pos != NULL && cmd_end >= 0) {
                // Find the start of the command (skip any leading \n or \r\n)
                int cmd_start = 0;
                while (cmd_start < cmd_end && (g_read_buffer[cmd_start] == '\n' || g_read_buffer[cmd_start] == '\r')) {
                    cmd_start++;
                }
                
                int cmd_len = cmd_end - cmd_start;
                
                if (cmd_len > 0 && cmd_len < (int)size) {
                    // Extract the command
                    strncpy(buffer, g_read_buffer + cmd_start, cmd_len);
                    buffer[cmd_len] = '\0';
                    
                    if (g_config.verbose) {
                        LOG_MESSAGE_DEBUG("Found command: '%s' (len: %d)", buffer, cmd_len);
                    }
                    
                    // Remove processed data including the line ending
                    int chars_to_remove = cmd_end;
                    // Skip the line ending characters
                    if (chars_to_remove < g_buffer_pos - 1 && g_read_buffer[chars_to_remove] == '\r' && g_read_buffer[chars_to_remove + 1] == '\n') {
                        chars_to_remove += 2; // Skip \r\n
                    } else if (chars_to_remove < g_buffer_pos && g_read_buffer[chars_to_remove] == '\n') {
                        chars_to_remove += 1; // Skip \n
                    }
                    
                    if (chars_to_remove < g_buffer_pos) {
                        memmove(g_read_buffer, g_read_buffer + chars_to_remove, g_buffer_pos - chars_to_remove);
                        g_buffer_pos -= chars_to_remove;
                        g_read_buffer[g_buffer_pos] = '\0';
                    } else {
                        g_buffer_pos = 0;
                        g_read_buffer[0] = '\0';
                    }
                    
                    return cmd_len;
                } else {
                    // Command too long or empty - skip this segment
                    if (g_config.verbose) {
                        LOG_MESSAGE_DEBUG("Skipping invalid command segment (len: %d)", cmd_len);
                    }
                    
                    // Remove up to and including the line ending
                    int chars_to_remove = cmd_end;
                    if (chars_to_remove < g_buffer_pos - 1 && g_read_buffer[chars_to_remove] == '\r' && g_read_buffer[chars_to_remove + 1] == '\n') {
                        chars_to_remove += 2;
                    } else if (chars_to_remove < g_buffer_pos && g_read_buffer[chars_to_remove] == '\n') {
                        chars_to_remove += 1;
                    }
                    
                    if (chars_to_remove < g_buffer_pos) {
                        memmove(g_read_buffer, g_read_buffer + chars_to_remove, g_buffer_pos - chars_to_remove);
                        g_buffer_pos -= chars_to_remove;
                        g_read_buffer[g_buffer_pos] = '\0';
                    } else {
                        g_buffer_pos = 0;
                        g_read_buffer[0] = '\0';
                    }
                    
                    // Try again with remaining buffer
                    return serial_read_complete_command(fd, buffer, size, timeout_sec);
                }
            }
            
            return 0;  // Need more data
        }
        
        return -1;  // Read error
    } else if (select_result == 0) {
        // Timeout occurred
        return 0;
    } else {
        // Error occurred
        LOG_MESSAGE_ERR("Select error: %s", strerror(errno));
        return -1;
    }
}

/**
 * Close serial port
 */
void serial_close(int fd) {
    if (fd >= 0) {
        serial_clear_buffers(fd);
        close(fd);
    }
}
