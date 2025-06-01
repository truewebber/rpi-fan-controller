/**
 * Temperature monitoring module for Fan Temperature Daemon
 * Handles CPU and NVME temperature reading
 */

#include "temperature.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * Get CPU temperature using vcgencmd
 */
float temperature_get_cpu(const char *cmd) {
    FILE *fp;
    char result[64];
    float temp = 61.0;  // Default fallback value
    
    if (cmd == NULL) {
        LOG_MESSAGE_ERR("CPU temperature command is NULL");
        return temp;
    }
    
    // Execute command to get CPU temperature
    fp = popen(cmd, "r");
    if (fp == NULL) {
        LOG_MESSAGE_ERR("Failed to run CPU temperature command: %s", strerror(errno));
        return temp;
    }
    
    // Read the output
    if (fgets(result, sizeof(result), fp) != NULL) {
        // Parse the temperature value (format: temp=XX.X'C)
        char *temp_str = strstr(result, "temp=");
        if (temp_str != NULL) {
            float parsed_temp;
            if (sscanf(temp_str + 5, "%f", &parsed_temp) == 1) {
                if (parsed_temp > 0 && parsed_temp < 120) {  // Sanity check
                    temp = parsed_temp;
                }
            }
        }
    }
    
    pclose(fp);
    return temp;
}

/**
 * Get NVME temperature using smartctl
 */
float temperature_get_nvme(const char *cmd) {
    FILE *fp;
    char line[256];
    float temp = 59.0;  // Default value
    
    if (cmd == NULL) {
        LOG_MESSAGE_ERR("NVME temperature command is NULL");
        return temp;
    }
    
    // Execute command to get NVME temperature
    fp = popen(cmd, "r");
    if (fp == NULL) {
        LOG_MESSAGE_ERR("Failed to run NVME temperature command: %s", strerror(errno));
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
 * Format temperature response string
 */
int temperature_format_response(char *buffer, size_t size, float cpu_temp, float nvme_temp) {
    if (buffer == NULL || size == 0) {
        return -1;
    }
    
    return snprintf(buffer, size, "CPU:%.2f|NVME:%.2f\n", cpu_temp, nvme_temp);
}
