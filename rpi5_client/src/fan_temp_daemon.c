/**
 * Raspberry Pi 5 Fan Temperature Daemon
 * 
 * This daemon monitors CPU and NVME temperatures on a Raspberry Pi 5
 * and responds to POLL requests from an Arduino-based fan controller.
 * 
 * Communication is via serial port (UART) at 115200 baud.
 * When the daemon receives "POLL", it responds with "CPU:xx.xx|NVME:xx.xx"
 * 
 * License: MIT
 */

/* 
 * Note: This code uses Linux-specific headers that may not be available
 * during development on non-Linux platforms like macOS or Windows.
 * These headers (termios.h, syslog.h, sys/stat.h) are standard on
 * Raspberry Pi OS and other Linux distributions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>  /* Linux-specific serial port control */
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>   /* Linux-specific syslog facility */
#include <sys/stat.h> /* Linux-specific file mode definitions */
#include <sys/ioctl.h> /* Linux-specific ioctl facility - not available on macOS during development */

// Environment variable names
#define ENV_SERIAL_PORT     "FAN_TEMP_SERIAL_PORT"
#define ENV_BAUD_RATE       "FAN_TEMP_BAUD_RATE"
#define ENV_READ_TIMEOUT    "FAN_TEMP_READ_TIMEOUT"
#define ENV_LOG_TO_SYSLOG   "FAN_TEMP_LOG_TO_SYSLOG"
#define ENV_CPU_TEMP_CMD    "FAN_TEMP_CPU_CMD"
#define ENV_NVME_TEMP_CMD   "FAN_TEMP_NVME_CMD"
#define ENV_FOREGROUND      "FAN_TEMP_FOREGROUND"
#define ENV_VERBOSE         "FAN_TEMP_VERBOSE"

// Global variables
static int running = 1;
static int serial_fd = -1;
static char *serial_port = NULL;
static int verbose = 0;
static int log_to_syslog = 0;
static speed_t baud_rate = B0;  // B0 is an invalid baud rate
static int read_timeout_sec = 0;
static char *cpu_temp_cmd = NULL;
static char *nvme_temp_cmd = NULL;
static int foreground = 0;

// Function prototypes
float get_cpu_temperature(void);
float get_nvme_temperature(void);
int setup_serial(const char *port);
int send_data(int fd, const char *data);
int read_data(int fd, char *buffer, size_t size);
int read_complete_command(int fd, char *buffer, size_t size);
void signal_handler(int sig);
void daemonize(void);
void cleanup(void);
void log_message(int priority, const char *format, ...);
void load_environment_config(void);
speed_t parse_baud_rate(const char *baud_str);
int check_required_env_vars(void);
void run_daemon(void);
void clean_buffer(char *buffer);
void clear_serial_buffers(int fd);
int check_serial_health(int fd);
void reset_read_buffer(void);
void recover_serial_synchronization(int fd);

/**
 * Check if all required environment variables are set
 * @return 0 if all required environment variables are set, 1 otherwise
 */
int check_required_env_vars(void) {
    int missing = 0;
    
    if (getenv(ENV_SERIAL_PORT) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_SERIAL_PORT);
        missing = 1;
    }
    
    if (getenv(ENV_BAUD_RATE) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_BAUD_RATE);
        missing = 1;
    }
    
    if (getenv(ENV_READ_TIMEOUT) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_READ_TIMEOUT);
        missing = 1;
    }
    
    if (getenv(ENV_LOG_TO_SYSLOG) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_LOG_TO_SYSLOG);
        missing = 1;
    }
    
    if (getenv(ENV_CPU_TEMP_CMD) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_CPU_TEMP_CMD);
        missing = 1;
    }
    
    if (getenv(ENV_NVME_TEMP_CMD) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_NVME_TEMP_CMD);
        missing = 1;
    }
    
    if (getenv(ENV_FOREGROUND) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_FOREGROUND);
        missing = 1;
    }
    
    if (getenv(ENV_VERBOSE) == NULL) {
        fprintf(stderr, "Error: %s environment variable is not set\n", ENV_VERBOSE);
        missing = 1;
    }
    
    return missing;
}

/**
 * Load configuration from environment variables
 */
void load_environment_config(void) {
    const char *env_val;
    
    // Load serial port
    env_val = getenv(ENV_SERIAL_PORT);
    if (env_val != NULL) {
        serial_port = strdup(env_val);
    }
    
    // Load baud rate
    env_val = getenv(ENV_BAUD_RATE);
    if (env_val != NULL) {
        baud_rate = parse_baud_rate(env_val);
    }
    
    // Load read timeout
    env_val = getenv(ENV_READ_TIMEOUT);
    if (env_val != NULL) {
        read_timeout_sec = atoi(env_val);
    }
    
    // Load log to syslog
    env_val = getenv(ENV_LOG_TO_SYSLOG);
    if (env_val != NULL) {
        log_to_syslog = atoi(env_val);
    }
    
    // Load CPU temperature command
    env_val = getenv(ENV_CPU_TEMP_CMD);
    if (env_val != NULL) {
        cpu_temp_cmd = strdup(env_val);
    }
    
    // Load NVME temperature command
    env_val = getenv(ENV_NVME_TEMP_CMD);
    if (env_val != NULL) {
        nvme_temp_cmd = strdup(env_val);
    }
    
    // Load foreground mode
    env_val = getenv(ENV_FOREGROUND);
    if (env_val != NULL) {
        foreground = atoi(env_val);
    }
    
    // Load verbose mode
    env_val = getenv(ENV_VERBOSE);
    if (env_val != NULL) {
        verbose = atoi(env_val);
    }
    
    // Check if all required environment variables are set
    if (check_required_env_vars()) {
        fprintf(stderr, "\nPlease set all required environment variables before running the daemon.\n");
        fprintf(stderr, "Example:\n");
        fprintf(stderr, "  export %s=/dev/serial0\n", ENV_SERIAL_PORT);
        fprintf(stderr, "  export %s=115200\n", ENV_BAUD_RATE);
        fprintf(stderr, "  export %s=1\n", ENV_READ_TIMEOUT);
        fprintf(stderr, "  export %s=1\n", ENV_LOG_TO_SYSLOG);
        fprintf(stderr, "  export %s=\"/usr/bin/vcgencmd measure_temp\"\n", ENV_CPU_TEMP_CMD);
        fprintf(stderr, "  export %s=\"smartctl -A /dev/nvme0 | grep Temperature\"\n", ENV_NVME_TEMP_CMD);
        fprintf(stderr, "  export %s=0\n", ENV_FOREGROUND);
        fprintf(stderr, "  export %s=0\n", ENV_VERBOSE);
        exit(EXIT_FAILURE);
    }
}

/**
 * Parse baud rate string to termios speed_t value
 */
speed_t parse_baud_rate(const char *baud_str) {
    long baud = atol(baud_str);
    
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B0;  // Invalid baud rate
    }
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    // Load configuration from environment variables
    load_environment_config();
    
    // Check if we should run in foreground mode
    if (getenv(ENV_FOREGROUND) != NULL && atoi(getenv(ENV_FOREGROUND)) == 1) {
        foreground = 1;
    }
    
    // Check if we should enable verbose logging
    if (getenv(ENV_VERBOSE) != NULL && atoi(getenv(ENV_VERBOSE)) == 1) {
        verbose = 1;
    }
    
    // Initialize logging
    if (log_to_syslog) {
        openlog("fan_temp_daemon", LOG_PID, LOG_DAEMON);
        log_message(LOG_INFO, "Fan temperature daemon starting");
    } else {
        log_message(LOG_INFO, "Fan temperature daemon starting");
    }
    
    // Run in foreground or background as requested
    if (!foreground) {
        log_message(LOG_INFO, "Daemonizing...");
        daemonize();
    } else {
        log_message(LOG_INFO, "Running in foreground");
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Main loop
    run_daemon();
    
    // Clean up
    if (log_to_syslog) {
        closelog();
    }
    
    return EXIT_SUCCESS;
}

/**
 * Get CPU temperature using vcgencmd
 */
float get_cpu_temperature(void) {
    FILE *fp;
    char result[64];
    float temp = 0.0;
    
    // Execute command to get CPU temperature
    fp = popen(cpu_temp_cmd, "r");
    if (fp == NULL) {
        log_message(LOG_ERR, "Failed to run CPU temperature command: %s", strerror(errno));
        return 50.0;  // Return default value on error
    }
    
    // Read the output
    if (fgets(result, sizeof(result), fp) != NULL) {
        // Parse the temperature value (format: temp=XX.X'C)
        char *temp_str = strstr(result, "temp=");
        if (temp_str != NULL) {
            sscanf(temp_str + 5, "%f", &temp);
        }
    }
    
    pclose(fp);
    return temp;
}

/**
 * Get NVME temperature using smartctl
 */
float get_nvme_temperature(void) {
    FILE *fp;
    char line[256];
    float temp = 59.0;  // Default value
    
    // Execute command to get NVME temperature
    fp = popen(nvme_temp_cmd, "r");
    if (fp == NULL) {
        log_message(LOG_ERR, "Failed to run NVME temperature command: %s", strerror(errno));
        return temp;
    }
    
    // Read the output and parse temperature
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Look for line starting with "Temperature:"
        if (strncmp(line, "Temperature:", 12) == 0) {
            // Find the temperature value after "Temperature:"
            char *temp_str = line + 12;  // Skip "Temperature:"
            
            // Skip whitespace
            while (*temp_str == ' ' || *temp_str == '\t') {
                temp_str++;
            }
            
            // Parse the temperature value
            float parsed_temp = atof(temp_str);
            if (parsed_temp > 0 && parsed_temp < 150) {  // Sanity check (0-150°C)
                temp = parsed_temp;
                break;  // Found the temperature, stop reading
            }
        }
    }
    
    pclose(fp);
    return temp;
}

/**
 * Configure and open serial port
 */
int setup_serial(const char *port) {
    int fd;
    struct termios tty;
    
    // Open serial port with additional flags for reliability
    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) {
        log_message(LOG_ERR, "Error opening serial port: %s", strerror(errno));
        return -1;
    }
    
    // Remove non-blocking flag after opening (we'll use select() for timeouts)
    int flags = fcntl(fd, F_GETFL);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    
    // Clear any existing data in buffers
    clear_serial_buffers(fd);
    
    // Get current settings
    if (tcgetattr(fd, &tty) != 0) {
        log_message(LOG_ERR, "Error from tcgetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }
    
    // Save original settings
    struct termios original_tty = tty;
    
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
    tty.c_cc[VTIME] = 2;  // 0.2 second timeout (more responsive than before)
    
    // Set terminal attributes immediately
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        log_message(LOG_ERR, "Error from tcsetattr: %s", strerror(errno));
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
            log_message(LOG_WARNING, "Serial port settings may not have been applied correctly");
        }
    }
    
    // Additional hardware-specific optimizations for Raspberry Pi
    // Set kernel buffer sizes for more reliable communication
    int buffer_size = 8192;  // 8KB buffers (increased from 4KB)
    if (ioctl(fd, TIOCOUTQ, &buffer_size) == 0) {
        if (verbose) {
            log_message(LOG_DEBUG, "Set output buffer size to %d bytes", buffer_size);
        }
    }
    
    // Disable exclusive mode to prevent locking issues
    if (ioctl(fd, TIOCNXCL) == -1) {
        if (verbose) {
            log_message(LOG_DEBUG, "Could not disable exclusive mode: %s", strerror(errno));
        }
    }
    
    // Clear buffers again after configuration
    clear_serial_buffers(fd);
    
    // Perform synchronization recovery
    recover_serial_synchronization(fd);
    
    return fd;
}

/**
 * Send data to serial port
 */
int send_data(int fd, const char *data) {
    return write(fd, data, strlen(data));
}

/**
 * Read data from serial port (blocking with timeout)
 */
int read_data(int fd, char *buffer, size_t size) {
    fd_set rdset;
    struct timeval timeout;
    
    // Set up select() for blocking read with timeout
    FD_ZERO(&rdset);
    FD_SET(fd, &rdset);
    
    // Set timeout
    timeout.tv_sec = read_timeout_sec;
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
        log_message(LOG_ERR, "Select error: %s", strerror(errno));
        return -1;
    }
}

/**
 * Reset the internal buffer of read_complete_command
 * Call this when reconnecting to serial port
 */
void reset_read_buffer(void) {
    // Force buffer reset by calling read_complete_command with fd = -1
    char dummy[1];
    read_complete_command(-1, dummy, 1);  // This will reset internal state
}

/**
 * Read complete command from serial port (buffered with timeout)
 * This function accumulates data and extracts commands ending with \r\n or \n
 * Algorithm: command = text ending with \r\n or \n
 */
int read_complete_command(int fd, char *buffer, size_t size) {
    static char read_buffer[512] = {0};  // Static buffer to accumulate data
    static int buffer_pos = 0;           // Current position in buffer
    
    // Special case: reset buffer when fd = -1
    if (fd < 0) {
        buffer_pos = 0;
        memset(read_buffer, 0, sizeof(read_buffer));
        if (verbose) {
            log_message(LOG_DEBUG, "Read buffer reset");
        }
        return 0;
    }
    
    fd_set rdset;
    struct timeval timeout;
    char temp_buf[64];
    
    // Set up select() for blocking read with timeout
    FD_ZERO(&rdset);
    FD_SET(fd, &rdset);
    
    // Set timeout
    timeout.tv_sec = read_timeout_sec;
    timeout.tv_usec = 0;
    
    // Wait for data with timeout
    int select_result = select(fd + 1, &rdset, NULL, NULL, &timeout);
    
    if (select_result > 0) {
        // Data is available, read it
        int bytes_read = read(fd, temp_buf, sizeof(temp_buf) - 1);
        
        if (bytes_read > 0) {
            temp_buf[bytes_read] = '\0';  // Null-terminate
            
            if (verbose) {
                // Log raw bytes in hex for debugging
                char hex_log[256];
                int hex_pos = 0;
                hex_pos += snprintf(hex_log + hex_pos, sizeof(hex_log) - hex_pos, "Raw hex: ");
                for (int i = 0; i < bytes_read && hex_pos < (int)sizeof(hex_log) - 4; i++) {
                    hex_pos += snprintf(hex_log + hex_pos, sizeof(hex_log) - hex_pos, "%02X ", (unsigned char)temp_buf[i]);
                }
                log_message(LOG_DEBUG, "%s", hex_log);
                
                log_message(LOG_DEBUG, "Raw data received: %d bytes", bytes_read);
            }
            
            // Add new data to our buffer, handling overflow
            for (int i = 0; i < bytes_read; i++) {
                if (buffer_pos < (int)(sizeof(read_buffer) - 1)) {
                    read_buffer[buffer_pos++] = temp_buf[i];
                } else {
                    // Buffer overflow - shift left and add new char
                    memmove(read_buffer, read_buffer + 1, sizeof(read_buffer) - 2);
                    read_buffer[sizeof(read_buffer) - 2] = temp_buf[i];
                    buffer_pos = sizeof(read_buffer) - 1;
                }
            }
            read_buffer[buffer_pos] = '\0';
            
            if (verbose) {
                // Create a clean version of buffer for logging
                char clean_log[128];
                int clean_pos = 0;
                for (int i = 0; i < buffer_pos && i < (int)sizeof(read_buffer) && clean_pos < 120; i++) {
                    if (read_buffer[i] == '\n') {
                        clean_log[clean_pos++] = '\\';
                        clean_log[clean_pos++] = 'n';
                    } else if (read_buffer[i] == '\r') {
                        clean_log[clean_pos++] = '\\';
                        clean_log[clean_pos++] = 'r';
                    } else if (read_buffer[i] >= 32 && read_buffer[i] <= 126) {
                        clean_log[clean_pos++] = read_buffer[i];
                    } else {
                        clean_log[clean_pos++] = '?';
                    }
                }
                clean_log[clean_pos] = '\0';
                log_message(LOG_DEBUG, "Buffer now: '%s' (%d chars)", clean_log, buffer_pos);
            }
            
            // Look for command ending with \r\n or \n
            char *newline_pos = NULL;
            int cmd_end = -1;
            
            // Search for \r\n first (Arduino format)
            for (int i = 0; i < buffer_pos - 1; i++) {
                if (read_buffer[i] == '\r' && read_buffer[i + 1] == '\n') {
                    newline_pos = &read_buffer[i];
                    cmd_end = i;
                    break;
                }
            }
            
            // If no \r\n found, look for standalone \n
            if (newline_pos == NULL) {
                for (int i = 0; i < buffer_pos; i++) {
                    if (read_buffer[i] == '\n') {
                        newline_pos = &read_buffer[i];
                        cmd_end = i;
                        break;
                    }
                }
            }
            
            if (newline_pos != NULL && cmd_end >= 0) {
                // Find the start of the command (skip any leading \n or \r\n)
                int cmd_start = 0;
                while (cmd_start < cmd_end && (read_buffer[cmd_start] == '\n' || read_buffer[cmd_start] == '\r')) {
                    cmd_start++;
                }
                
                int cmd_len = cmd_end - cmd_start;
                
                if (cmd_len > 0 && cmd_len < (int)size) {
                    // Extract the command
                    strncpy(buffer, read_buffer + cmd_start, cmd_len);
                    buffer[cmd_len] = '\0';
                    
                    if (verbose) {
                        log_message(LOG_DEBUG, "Found command: '%s' (len: %d)", buffer, cmd_len);
                    }
                    
                    // Remove processed data including the line ending
                    int chars_to_remove = cmd_end;
                    // Skip the line ending characters
                    if (chars_to_remove < buffer_pos - 1 && read_buffer[chars_to_remove] == '\r' && read_buffer[chars_to_remove + 1] == '\n') {
                        chars_to_remove += 2; // Skip \r\n
                    } else if (chars_to_remove < buffer_pos && read_buffer[chars_to_remove] == '\n') {
                        chars_to_remove += 1; // Skip \n
                    }
                    
                    if (chars_to_remove < buffer_pos) {
                        memmove(read_buffer, read_buffer + chars_to_remove, buffer_pos - chars_to_remove);
                        buffer_pos -= chars_to_remove;
                        read_buffer[buffer_pos] = '\0';
                    } else {
                        buffer_pos = 0;
                        read_buffer[0] = '\0';
                    }
                    
                    if (verbose) {
                        char clean_remaining[64];
                        int clean_pos = 0;
                        for (int i = 0; i < buffer_pos && clean_pos < 60; i++) {
                            if (read_buffer[i] == '\n') {
                                clean_remaining[clean_pos++] = '\\';
                                clean_remaining[clean_pos++] = 'n';
                            } else if (read_buffer[i] == '\r') {
                                clean_remaining[clean_pos++] = '\\';
                                clean_remaining[clean_pos++] = 'r';
                            } else if (read_buffer[i] >= 32 && read_buffer[i] <= 126) {
                                clean_remaining[clean_pos++] = read_buffer[i];
                            } else {
                                clean_remaining[clean_pos++] = '?';
                            }
                        }
                        clean_remaining[clean_pos] = '\0';
                        log_message(LOG_DEBUG, "Buffer after extraction: '%s' (%d chars)", clean_remaining, buffer_pos);
                    }
                    
                    return cmd_len;
                } else {
                    // Command too long or empty - skip this segment
                    if (verbose) {
                        log_message(LOG_DEBUG, "Skipping invalid command segment (len: %d)", cmd_len);
                    }
                    
                    // Remove up to and including the line ending
                    int chars_to_remove = cmd_end;
                    if (chars_to_remove < buffer_pos - 1 && read_buffer[chars_to_remove] == '\r' && read_buffer[chars_to_remove + 1] == '\n') {
                        chars_to_remove += 2;
                    } else if (chars_to_remove < buffer_pos && read_buffer[chars_to_remove] == '\n') {
                        chars_to_remove += 1;
                    }
                    
                    if (chars_to_remove < buffer_pos) {
                        memmove(read_buffer, read_buffer + chars_to_remove, buffer_pos - chars_to_remove);
                        buffer_pos -= chars_to_remove;
                        read_buffer[buffer_pos] = '\0';
                    } else {
                        buffer_pos = 0;
                        read_buffer[0] = '\0';
                    }
                    
                    // Try again with remaining buffer
                    return read_complete_command(fd, buffer, size);
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
        log_message(LOG_ERR, "Select error: %s", strerror(errno));
        return -1;
    }
}

/**
 * Signal handler
 */
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            log_message(LOG_INFO, "Received signal %d, shutting down", sig);
            running = 0;
            break;
        case SIGHUP:
            log_message(LOG_INFO, "Received SIGHUP, reloading configuration");
            // Reload configuration if needed
            break;
    }
}

/**
 * Daemonize the process
 */
void daemonize(void) {
    pid_t pid, sid;
    
    // Fork off the parent process
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    // If we got a good PID, then we can exit the parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Change the file mode mask
    umask(0);
    
    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
    
    // Change the current working directory
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

/**
 * Cleanup resources
 */
void cleanup(void) {
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
    
    closelog();
}

/**
 * Log message to syslog or stdout
 */
void log_message(int priority, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    if (log_to_syslog) {
        vsyslog(priority, format, args);
    } else {
        vprintf(format, args);
        printf("\n");
    }
    
    va_end(args);
}

/**
 * Clean received buffer by removing whitespace and line endings
 */
void clean_buffer(char *buffer) {
    char *end;
    char *start = buffer;
    
    // Remove trailing whitespace and line endings
    end = buffer + strlen(buffer) - 1;
    while (end > buffer && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    // Remove leading whitespace - FIXED: move content instead of pointer
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }
    
    // If we found leading whitespace, move the content
    if (start != buffer) {
        memmove(buffer, start, strlen(start) + 1);  // +1 for null terminator
    }
}

/**
 * Clear serial port buffers
 */
void clear_serial_buffers(int fd) {
    tcflush(fd, TCIOFLUSH);  // Clear both input and output buffers
}

/**
 * Check if serial connection is healthy
 */
int check_serial_health(int fd) {
    int status;
    if (ioctl(fd, TIOCMGET, &status) == -1) {
        return 0;  // Connection problem
    }
    return 1;  // Connection OK
}

/**
 * Aggressively clear serial port and recover synchronization
 */
void recover_serial_synchronization(int fd) {
    char discard_buffer[256];
    int attempts = 0;
    
    if (verbose) {
        log_message(LOG_DEBUG, "Starting serial synchronization recovery");
    }
    
    // Clear hardware buffers multiple times with longer delays
    for (int i = 0; i < 5; i++) {
        tcflush(fd, TCIOFLUSH);
        usleep(200000);  // 200ms between flushes
    }
    
    // Send a few dummy bytes to trigger any pending Arduino transmissions
    const char *sync_chars = "\n\n\n";
    write(fd, sync_chars, strlen(sync_chars));
    fsync(fd);  // Force data to be sent
    usleep(300000);  // 300ms to let Arduino respond
    
    // Clear buffers again after sync attempt
    tcflush(fd, TCIOFLUSH);
    usleep(100000);  // 100ms
    
    // Read and discard any remaining data with timeout
    fd_set rdset;
    struct timeval timeout;
    
    while (attempts < 20) {  // Max 20 attempts (increased from 10)
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms timeout (decreased for faster cleanup)
        
        int select_result = select(fd + 1, &rdset, NULL, NULL, &timeout);
        
        if (select_result > 0) {
            int bytes_discarded = read(fd, discard_buffer, sizeof(discard_buffer) - 1);
            if (bytes_discarded > 0) {
                discard_buffer[bytes_discarded] = '\0';
                if (verbose) {
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
                    log_message(LOG_DEBUG, "Discarded stale data: '%s' (%d bytes)", clean_discard, bytes_discarded);
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
    tcflush(fd, TCIOFLUSH);
    usleep(100000);  // 100ms
    tcflush(fd, TCIOFLUSH);
    usleep(50000);   // 50ms
    
    // Reset internal read buffer
    reset_read_buffer();
    
    if (verbose) {
        log_message(LOG_DEBUG, "Serial synchronization recovery completed (%d cleanup attempts)", attempts);
    }
}

/**
 * Run the daemon
 */
void run_daemon(void) {
    char buffer[256];
    char temp_data[64];
    int consecutive_errors = 0;
    int consecutive_timeouts = 0;
    int successful_exchanges = 0;
    int startup_sync_mode = 1;  // Flag to ignore incomplete commands during startup
    
    // Open and configure serial port
    serial_fd = setup_serial(serial_port);
    if (serial_fd < 0) {
        log_message(LOG_ERR, "Failed to open serial port %s", serial_port);
        cleanup();
        return;
    }
    
    log_message(LOG_INFO, "Temperature monitoring started on %s (baud: %ld, timeout: %ds)", 
                serial_port, 
                baud_rate == B115200 ? 115200 : 
                baud_rate == B57600 ? 57600 : 
                baud_rate == B38400 ? 38400 : 
                baud_rate == B19200 ? 19200 : 9600,
                read_timeout_sec);
    
    // Main loop
    while (running) {
        // Check connection health only after many consecutive timeouts (not during active communication)
        if (consecutive_timeouts > 30 && successful_exchanges == 0) {
            if (!check_serial_health(serial_fd)) {
                log_message(LOG_WARNING, "Serial port health check failed after %d timeouts, attempting reconnection", consecutive_timeouts);
                close(serial_fd);
                
                // Reset read buffer to clear any partial data
                reset_read_buffer();
                
                serial_fd = setup_serial(serial_port);
                if (serial_fd < 0) {
                    log_message(LOG_ERR, "Failed to reconnect to serial port");
                    sleep(5);  // Wait before retry
                    continue;
                }
                consecutive_errors = 0;
                consecutive_timeouts = 0;  // Reset timeout counter
                startup_sync_mode = 1;  // Re-enter sync mode after reconnection
                
                // Add a brief delay after reconnection to let things stabilize
                usleep(500000);  // 500ms
            }
        }
        
        // Wait for a command from the fan controller (blocking with timeout)
        int bytes_read = read_complete_command(serial_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';  // Null-terminate the string
            consecutive_errors = 0;  // Reset error counter on successful read
            consecutive_timeouts = 0;  // Reset timeout counter on successful read
            
            if (verbose) {
                log_message(LOG_DEBUG, "Received command: '%s' (length: %d)", buffer, bytes_read);
            }
            
            // Clean the buffer before comparison
            clean_buffer(buffer);
            
            if (verbose) {
                log_message(LOG_DEBUG, "Cleaned command: '%s'", buffer);
            }
            
            if (strcmp(buffer, "POLL") == 0) {
                // Exit startup sync mode on first valid POLL command
                if (startup_sync_mode) {
                    startup_sync_mode = 0;
                    log_message(LOG_INFO, "Serial synchronization established - normal operation begins");
                }
                
                // Get current temperatures
                float cpu_temp = get_cpu_temperature();
                float nvme_temp = get_nvme_temperature();
                
                // Format temperature data
                snprintf(temp_data, sizeof(temp_data), "CPU:%.2f|NVME:%.2f\n", cpu_temp, nvme_temp);
                
                // Send temperature data
                int sent = send_data(serial_fd, temp_data);
                
                if (verbose) {
                    log_message(LOG_DEBUG, "Sent: %s (bytes: %d)", temp_data, sent);
                }
                
                // Count successful exchange
                successful_exchanges++;
                
                // Reset successful exchanges counter periodically to allow recovery
                if (successful_exchanges > 10) {
                    successful_exchanges = 1;
                }
            } else if (strlen(buffer) == 0) {
                // Empty message (just line terminator) - this is normal, ignore silently
                if (verbose) {
                    log_message(LOG_DEBUG, "Received empty message - ignoring");
                }
            } else {
                // Handle non-empty unknown commands
                if (startup_sync_mode) {
                    // During startup, ignore unknown commands but DON'T clear buffers
                    // Let the accumulation algorithm work naturally
                    if (verbose) {
                        log_message(LOG_DEBUG, "Unknown command during startup sync: '%s' - ignoring but keeping buffer", buffer);
                    }
                    // NO buffer clearing - let data accumulate naturally
                } else {
                    // Normal mode - log unknown commands
                    if (verbose) {
                        log_message(LOG_DEBUG, "Unknown command received: '%s'", buffer);
                    }
                }
            }
        } else if (bytes_read < 0) {
            consecutive_errors++;
            successful_exchanges = 0;  // Reset on errors to allow health check
            
            if (verbose) {
                log_message(LOG_WARNING, "Error reading from serial port: %s (error count: %d)", 
                           strerror(errno), consecutive_errors);
            }
            
            // If too many consecutive errors, try to reconnect
            if (consecutive_errors >= 5) {
                log_message(LOG_WARNING, "Too many consecutive errors, attempting reconnection");
                clear_serial_buffers(serial_fd);  // Clear before closing
                close(serial_fd);
                
                // Reset read buffer to clear any partial data
                reset_read_buffer();
                
                serial_fd = setup_serial(serial_port);
                if (serial_fd < 0) {
                    log_message(LOG_ERR, "Failed to reconnect to serial port");
                    sleep(5);  // Wait before retry
                    continue;
                }
                consecutive_errors = 0;
                consecutive_timeouts = 0;  // Reset timeout counter
                startup_sync_mode = 1;  // Re-enter sync mode after reconnection
                
                // Add a brief delay after reconnection to let things stabilize
                usleep(500000);  // 500ms
            }
            
            // Error occurred, sleep briefly to avoid tight loop on error
            usleep(100000);  // 100ms
        } else {
            // Timeout occurred - this is normal
            consecutive_timeouts++;
            
            if (verbose && (consecutive_timeouts % 10 == 1)) {  // Log every 10th timeout to reduce spam
                log_message(LOG_DEBUG, "Timeout waiting for data from serial port (count: %d)", consecutive_timeouts);
            }
        }
    }
    
    // Cleanup and exit
    cleanup();
}
