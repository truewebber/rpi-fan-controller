#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stddef.h>

// Function prototypes
float temperature_get_cpu(const char *cmd);
float temperature_get_nvme(const char *cmd);
int temperature_format_response(char *buffer, size_t size, float cpu_temp, float nvme_temp);

#endif // TEMPERATURE_H
