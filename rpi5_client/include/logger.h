#ifndef LOGGER_H
#define LOGGER_H

#include <syslog.h>

// Function prototypes
int logger_init(int use_syslog);
void logger_cleanup(void);
void logger_log(int priority, const char *format, ...);

// Convenience macros (use system syslog constants)
#define LOG_MESSAGE_DEBUG(fmt, ...)    logger_log(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_MESSAGE_INFO(fmt, ...)     logger_log(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_MESSAGE_WARNING(fmt, ...)  logger_log(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOG_MESSAGE_ERR(fmt, ...)      logger_log(LOG_ERR, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
