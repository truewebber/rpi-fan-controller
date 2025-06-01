#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include <stddef.h>

// Function prototypes
int serial_setup(const char *port, speed_t baud_rate);
int serial_send_data(int fd, const char *data);
int serial_read_data(int fd, char *buffer, size_t size, int timeout_sec);
int serial_read_complete_command(int fd, char *buffer, size_t size, int timeout_sec);
void serial_clear_buffers(int fd);
int serial_check_health(int fd);
void serial_recover_synchronization(int fd);
void serial_reset_read_buffer(void);
void serial_close(int fd);

#endif // SERIAL_H
