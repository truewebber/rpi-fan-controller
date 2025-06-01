#ifndef DAEMON_H
#define DAEMON_H

// Global control variables
extern volatile int g_running;

// Function prototypes
void daemon_daemonize(void);
void daemon_setup_signals(void);
void daemon_cleanup(void);
void daemon_signal_handler(int sig);

#endif // DAEMON_H
