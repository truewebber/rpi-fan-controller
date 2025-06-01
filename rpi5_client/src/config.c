/**
 * Configuration module for Fan Temperature Daemon
 * Handles loading and validation of environment variables
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

// Global configuration instance
config_t g_config = {0};

/**
 * Parse baud rate string to termios speed_t value
 */
speed_t config_parse_baud_rate(const char *baud_str) {
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
 * Check if all required environment variables are set
 */
static int check_required_env_vars(void) {
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
int config_load_from_env(void) {
    const char *env_val;
    
    // Check if all required environment variables are set
    if (check_required_env_vars()) {
        config_print_usage();
        return -1;
    }
    
    // Load serial port
    env_val = getenv(ENV_SERIAL_PORT);
    if (env_val != NULL) {
        g_config.serial_port = strdup(env_val);
    }
    
    // Load baud rate
    env_val = getenv(ENV_BAUD_RATE);
    if (env_val != NULL) {
        g_config.baud_rate = config_parse_baud_rate(env_val);
        if (g_config.baud_rate == B0) {
            fprintf(stderr, "Error: Invalid baud rate: %s\n", env_val);
            return -1;
        }
    }
    
    // Load read timeout
    env_val = getenv(ENV_READ_TIMEOUT);
    if (env_val != NULL) {
        g_config.read_timeout_sec = atoi(env_val);
        if (g_config.read_timeout_sec <= 0) {
            fprintf(stderr, "Error: Invalid read timeout: %s\n", env_val);
            return -1;
        }
    }
    
    // Load log to syslog
    env_val = getenv(ENV_LOG_TO_SYSLOG);
    if (env_val != NULL) {
        g_config.log_to_syslog = atoi(env_val);
    }
    
    // Load CPU temperature command
    env_val = getenv(ENV_CPU_TEMP_CMD);
    if (env_val != NULL) {
        g_config.cpu_temp_cmd = strdup(env_val);
    }
    
    // Load NVME temperature command
    env_val = getenv(ENV_NVME_TEMP_CMD);
    if (env_val != NULL) {
        g_config.nvme_temp_cmd = strdup(env_val);
    }
    
    // Load foreground mode
    env_val = getenv(ENV_FOREGROUND);
    if (env_val != NULL) {
        g_config.foreground = atoi(env_val);
    }
    
    // Load verbose mode
    env_val = getenv(ENV_VERBOSE);
    if (env_val != NULL) {
        g_config.verbose = atoi(env_val);
    }
    
    return 0;
}

/**
 * Validate loaded configuration
 */
int config_validate(void) {
    if (g_config.serial_port == NULL || strlen(g_config.serial_port) == 0) {
        fprintf(stderr, "Error: Serial port not configured\n");
        return -1;
    }
    
    if (g_config.baud_rate == B0) {
        fprintf(stderr, "Error: Invalid baud rate\n");
        return -1;
    }
    
    if (g_config.read_timeout_sec <= 0) {
        fprintf(stderr, "Error: Invalid read timeout\n");
        return -1;
    }
    
    if (g_config.cpu_temp_cmd == NULL || strlen(g_config.cpu_temp_cmd) == 0) {
        fprintf(stderr, "Error: CPU temperature command not configured\n");
        return -1;
    }
    
    if (g_config.nvme_temp_cmd == NULL || strlen(g_config.nvme_temp_cmd) == 0) {
        fprintf(stderr, "Error: NVME temperature command not configured\n");
        return -1;
    }
    
    return 0;
}

/**
 * Print usage information
 */
void config_print_usage(void) {
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
}

/**
 * Cleanup configuration resources
 */
void config_cleanup(void) {
    if (g_config.serial_port) {
        free(g_config.serial_port);
        g_config.serial_port = NULL;
    }
    
    if (g_config.cpu_temp_cmd) {
        free(g_config.cpu_temp_cmd);
        g_config.cpu_temp_cmd = NULL;
    }
    
    if (g_config.nvme_temp_cmd) {
        free(g_config.nvme_temp_cmd);
        g_config.nvme_temp_cmd = NULL;
    }
}
