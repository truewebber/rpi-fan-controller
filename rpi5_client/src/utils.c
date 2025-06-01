/**
 * Utilities module for Fan Temperature Daemon
 * Contains helper and utility functions
 */

#include "utils.h"
#include <string.h>
#include <unistd.h>

/**
 * Clean received buffer by removing whitespace and line endings
 */
void utils_clean_buffer(char *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    char *end;
    char *start = buffer;
    
    // Remove trailing whitespace and line endings
    end = buffer + strlen(buffer) - 1;
    while (end > buffer && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    // Remove leading whitespace - move content instead of pointer
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }
    
    // If we found leading whitespace, move the content
    if (start != buffer) {
        memmove(buffer, start, strlen(start) + 1);  // +1 for null terminator
    }
}

/**
 * Sleep for specified milliseconds
 */
void utils_sleep_ms(int milliseconds) {
    if (milliseconds > 0) {
        usleep(milliseconds * 1000);
    }
}
