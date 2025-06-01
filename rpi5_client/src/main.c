/**
 * Main module for Fan Temperature Daemon
 * Coordinates all other modules and contains the main daemon loop
 */

#include "config.h"
#include "logger.h"
#include "daemon.h"
#include "serial.h"
#include "temperature.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/**
 * Main daemon loop
 */
static void run_main_loop(void) {
    char buffer[256];
    char temp_data[64];
    int consecutive_errors = 0;
    int consecutive_timeouts = 0;
    int successful_exchanges = 0;
    int startup_sync_mode = 1;  // Flag to ignore incomplete commands during startup
    int serial_fd;
    
    // Open and configure serial port
    serial_fd = serial_setup(g_config.serial_port, g_config.baud_rate);
    if (serial_fd < 0) {
        LOG_MESSAGE_ERR("Failed to open serial port %s", g_config.serial_port);
        return;
    }
    
    // Log startup information
    const char *baud_str = "unknown";
    switch (g_config.baud_rate) {
        case B9600:   baud_str = "9600"; break;
        case B19200:  baud_str = "19200"; break;
        case B38400:  baud_str = "38400"; break;
        case B57600:  baud_str = "57600"; break;
        case B115200: baud_str = "115200"; break;
    }
    
    LOG_MESSAGE_INFO("Temperature monitoring started on %s (baud: %s, timeout: %ds)", 
             g_config.serial_port, baud_str, g_config.read_timeout_sec);
    
    // Main loop
    while (g_running) {
        // Check connection health only after many consecutive timeouts
        if (consecutive_timeouts > 30 && successful_exchanges == 0) {
            if (!serial_check_health(serial_fd)) {
                LOG_MESSAGE_WARNING("Serial port health check failed after %d timeouts, attempting reconnection", consecutive_timeouts);
                serial_close(serial_fd);
                
                serial_fd = serial_setup(g_config.serial_port, g_config.baud_rate);
                if (serial_fd < 0) {
                    LOG_MESSAGE_ERR("Failed to reconnect to serial port");
                    sleep(5);  // Wait before retry
                    continue;
                }
                consecutive_errors = 0;
                consecutive_timeouts = 0;
                startup_sync_mode = 1;
                utils_sleep_ms(500);  // 500ms stabilization delay
            }
        }
        
        // Wait for a command from the fan controller
        int bytes_read = serial_read_complete_command(serial_fd, buffer, sizeof(buffer), g_config.read_timeout_sec);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            consecutive_errors = 0;
            consecutive_timeouts = 0;
            
            if (g_config.verbose) {
                LOG_MESSAGE_DEBUG("Received command: '%s' (length: %d)", buffer, bytes_read);
            }
            
            // Clean the buffer before comparison
            utils_clean_buffer(buffer);
            
            if (g_config.verbose) {
                LOG_MESSAGE_DEBUG("Cleaned command: '%s'", buffer);
            }
            
            if (strcmp(buffer, "POLL") == 0) {
                // Exit startup sync mode on first valid POLL command
                if (startup_sync_mode) {
                    startup_sync_mode = 0;
                    LOG_MESSAGE_INFO("Serial synchronization established - normal operation begins");
                }
                
                // Get current temperatures
                float cpu_temp = temperature_get_cpu(g_config.cpu_temp_cmd);
                float nvme_temp = temperature_get_nvme(g_config.nvme_temp_cmd);
                
                // Format temperature data
                int formatted = temperature_format_response(temp_data, sizeof(temp_data), cpu_temp, nvme_temp);
                
                if (formatted > 0) {
                    // Send temperature data
                    int sent = serial_send_data(serial_fd, temp_data);
                    
                    if (g_config.verbose) {
                        LOG_MESSAGE_DEBUG("Sent: %s (bytes: %d)", temp_data, sent);
                    }
                    
                    // Count successful exchange
                    successful_exchanges++;
                    
                    // Reset successful exchanges counter periodically
                    if (successful_exchanges > 10) {
                        successful_exchanges = 1;
                    }
                } else {
                    LOG_MESSAGE_ERR("Failed to format temperature response");
                }
            } else if (strlen(buffer) == 0) {
                // Empty message - ignore silently
                if (g_config.verbose) {
                    LOG_MESSAGE_DEBUG("Received empty message - ignoring");
                }
            } else {
                // Handle non-empty unknown commands
                if (startup_sync_mode) {
                    // During startup, ignore unknown commands
                    if (g_config.verbose) {
                        LOG_MESSAGE_DEBUG("Unknown command during startup sync: '%s' - ignoring", buffer);
                    }
                } else {
                    // Normal mode - log unknown commands
                    if (g_config.verbose) {
                        LOG_MESSAGE_DEBUG("Unknown command received: '%s'", buffer);
                    }
                }
            }
        } else if (bytes_read < 0) {
            consecutive_errors++;
            successful_exchanges = 0;
            
            if (g_config.verbose) {
                LOG_MESSAGE_WARNING("Error reading from serial port: %s (error count: %d)", 
                           strerror(errno), consecutive_errors);
            }
            
            // If too many consecutive errors, try to reconnect
            if (consecutive_errors >= 5) {
                LOG_MESSAGE_WARNING("Too many consecutive errors, attempting reconnection");
                serial_close(serial_fd);
                
                serial_fd = serial_setup(g_config.serial_port, g_config.baud_rate);
                if (serial_fd < 0) {
                    LOG_MESSAGE_ERR("Failed to reconnect to serial port");
                    sleep(5);  // Wait before retry
                    continue;
                }
                consecutive_errors = 0;
                consecutive_timeouts = 0;
                startup_sync_mode = 1;
                utils_sleep_ms(500);  // 500ms stabilization delay
            }
            
            // Sleep briefly to avoid tight loop on error
            utils_sleep_ms(100);
        } else {
            // Timeout occurred - this is normal
            consecutive_timeouts++;
            
            if (g_config.verbose && (consecutive_timeouts % 10 == 1)) {
                LOG_MESSAGE_DEBUG("Timeout waiting for data from serial port (count: %d)", consecutive_timeouts);
            }
        }
    }
    
    // Cleanup
    serial_close(serial_fd);
    LOG_MESSAGE_INFO("Main loop completed");
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    // Load configuration from environment variables
    if (config_load_from_env() != 0) {
        fprintf(stderr, "Failed to load configuration\n");
        return EXIT_FAILURE;
    }
    
    // Validate configuration
    if (config_validate() != 0) {
        fprintf(stderr, "Configuration validation failed\n");
        config_cleanup();
        return EXIT_FAILURE;
    }
    
    // Initialize logging
    if (logger_init(g_config.log_to_syslog) != 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        config_cleanup();
        return EXIT_FAILURE;
    }
    
    LOG_MESSAGE_INFO("Fan temperature daemon starting");
    
    // Daemonize if not in foreground mode
    daemon_daemonize();
    
    // Set up signal handlers
    daemon_setup_signals();
    
    // Run main daemon loop
    run_main_loop();
    
    // Cleanup
    daemon_cleanup();
    
    LOG_MESSAGE_INFO("Fan temperature daemon stopped");
    
    return EXIT_SUCCESS;
}
