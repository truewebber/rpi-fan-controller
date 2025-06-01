#ifndef CONFIG_H
#define CONFIG_H

#include <termios.h>

// Environment variable names
#define ENV_SERIAL_PORT     "FAN_TEMP_SERIAL_PORT"
#define ENV_BAUD_RATE       "FAN_TEMP_BAUD_RATE"
#define ENV_READ_TIMEOUT    "FAN_TEMP_READ_TIMEOUT"
#define ENV_LOG_TO_SYSLOG   "FAN_TEMP_LOG_TO_SYSLOG"
#define ENV_CPU_TEMP_CMD    "FAN_TEMP_CPU_CMD"
#define ENV_NVME_TEMP_CMD   "FAN_TEMP_NVME_CMD"
#define ENV_FOREGROUND      "FAN_TEMP_FOREGROUND"
#define ENV_VERBOSE         "FAN_TEMP_VERBOSE"

// Configuration structure
typedef struct {
    char *serial_port;
    speed_t baud_rate;
    int read_timeout_sec;
    int log_to_syslog;
    char *cpu_temp_cmd;
    char *nvme_temp_cmd;
    int foreground;
    int verbose;
} config_t;

// Global configuration instance
extern config_t g_config;

// Function prototypes
int config_load_from_env(void);
void config_cleanup(void);
int config_validate(void);
speed_t config_parse_baud_rate(const char *baud_str);
void config_print_usage(void);

#endif // CONFIG_H
