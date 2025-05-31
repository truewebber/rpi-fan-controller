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
void signal_handler(int sig);
void daemonize(void);
void cleanup(void);
void log_message(int priority, const char *format, ...);
void load_environment_config(void);
speed_t parse_baud_rate(const char *baud_str);
int check_required_env_vars(void);
void run_daemon(void);
void clean_buffer(char *buffer);

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
    float temp = 50.0;  // Default value
    
    // Execute command to get NVME temperature
    fp = popen(nvme_temp_cmd, "r");
    if (fp == NULL) {
        log_message(LOG_ERR, "Failed to run NVME temperature command: %s", strerror(errno));
        return temp;
    }
    
    // Read the output and parse temperature
    if (fgets(line, sizeof(line), fp) != NULL) {
        char *token = strtok(line, " ");
        int field_count = 0;
        
        // Temperature is typically in the 10th field
        while (token != NULL && field_count < 10) {
            token = strtok(NULL, " ");
            field_count++;
            
            if (field_count == 9 && token != NULL) {
                temp = atof(token);
                break;
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
    
    // Open serial port
    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        log_message(LOG_ERR, "Error opening serial port: %s", strerror(errno));
        return -1;
    }
    
    // Get current settings
    if (tcgetattr(fd, &tty) != 0) {
        log_message(LOG_ERR, "Error from tcgetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }
    
    // Set baud rate
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);
    
    // 8-bit chars, no parity, 1 stop bit
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;
    
    // No flow control
    tty.c_cflag &= ~(CRTSCTS);
    tty.c_cflag |= CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines
    
    // Set terminal attributes
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        log_message(LOG_ERR, "Error from tcsetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }
    
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
    
    // Remove trailing whitespace and line endings
    end = buffer + strlen(buffer) - 1;
    while (end > buffer && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    // Remove leading whitespace
    while (*buffer && (*buffer == ' ' || *buffer == '\t')) {
        buffer++;
    }
}

/**
 * Run the daemon
 */
void run_daemon(void) {
    char buffer[256];
    char temp_data[64];
    
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
        // Wait for a command from the fan controller (blocking with timeout)
        int bytes_read = read_data(serial_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';  // Null-terminate the string
            
            if (verbose) {
                log_message(LOG_DEBUG, "Received command: %s", buffer);
            }
            
            // Clean the buffer before comparison
            clean_buffer(buffer);
            
            if (strcmp(buffer, "POLL") == 0) {
                // Get current temperatures
                float cpu_temp = get_cpu_temperature();
                float nvme_temp = get_nvme_temperature();
                
                // Format temperature data
                snprintf(temp_data, sizeof(temp_data), "CPU:%.2f|NVME:%.2f\n", cpu_temp, nvme_temp);
                
                // Send temperature data
                send_data(serial_fd, temp_data);
                
                if (verbose) {
                    log_message(LOG_DEBUG, "Sent: %s", temp_data);
                }
            }
        } else if (bytes_read < 0) {
            log_message(LOG_ERR, "Error reading from serial port: %s", strerror(errno));
            // Error occurred, sleep briefly to avoid tight loop on error
            usleep(100000);  // 100ms
        }
    }
    
    // Cleanup and exit
    cleanup();
} 