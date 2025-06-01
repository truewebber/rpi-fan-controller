/**
 * Logger module for Fan Temperature Daemon
 * Provides unified logging interface for syslog and stdout
 */

#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

static int g_use_syslog = 0;

/**
 * Initialize logger
 */
int logger_init(int use_syslog) {
    g_use_syslog = use_syslog;
    
    if (g_use_syslog) {
        openlog("fan_temp_daemon", LOG_PID, LOG_DAEMON);
        logger_log(LOG_INFO, "Fan temperature daemon logging initialized (syslog)");
    } else {
        logger_log(LOG_INFO, "Fan temperature daemon logging initialized (stdout)");
    }
    
    return 0;
}

/**
 * Cleanup logger
 */
void logger_cleanup(void) {
    if (g_use_syslog) {
        logger_log(LOG_INFO, "Fan temperature daemon logging cleanup");
        closelog();
    }
}

/**
 * Log message with priority
 */
void logger_log(int priority, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    if (g_use_syslog) {
        vsyslog(priority, format, args);
    } else {
        // Add priority prefix for stdout logging
        const char *level_str;
        switch (priority) {
            case LOG_DEBUG:   level_str = "DEBUG"; break;
            case LOG_INFO:    level_str = "INFO"; break;
            case LOG_WARNING: level_str = "WARNING"; break;
            case LOG_ERR:     level_str = "ERROR"; break;
            default:          level_str = "UNKNOWN"; break;
        }
        
        printf("[%s] ", level_str);
        vprintf(format, args);
        printf("\n");
        fflush(stdout);
    }
    
    va_end(args);
}
