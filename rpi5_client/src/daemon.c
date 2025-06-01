/**
 * Daemon module for Fan Temperature Daemon
 * Handles process daemonization and signal management
 */

#include "daemon.h"
#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

// Global control variable
volatile int g_running = 1;

/**
 * Signal handler
 */
void daemon_signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            LOG_MESSAGE_INFO("Received signal %d, shutting down", sig);
            g_running = 0;
            break;
        case SIGHUP:
            LOG_MESSAGE_INFO("Received SIGHUP, reloading configuration");
            // TODO: Implement configuration reload if needed
            break;
        default:
            LOG_MESSAGE_WARNING("Received unexpected signal %d", sig);
            break;
    }
}

/**
 * Setup signal handlers
 */
void daemon_setup_signals(void) {
    signal(SIGINT, daemon_signal_handler);
    signal(SIGTERM, daemon_signal_handler);
    signal(SIGHUP, daemon_signal_handler);
}

/**
 * Daemonize the process
 */
void daemon_daemonize(void) {
    pid_t pid, sid;
    
    if (g_config.foreground) {
        LOG_MESSAGE_INFO("Running in foreground mode");
        return;
    }
    
    LOG_MESSAGE_INFO("Daemonizing process...");
    
    // Fork off the parent process
    pid = fork();
    if (pid < 0) {
        LOG_MESSAGE_ERR("Fork failed");
        exit(EXIT_FAILURE);
    }
    
    // If we got a good PID, then we can exit the parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Change the file mode mask
    umask(0);
    
    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        LOG_MESSAGE_ERR("setsid failed");
        exit(EXIT_FAILURE);
    }
    
    // Change the current working directory
    if (chdir("/") < 0) {
        LOG_MESSAGE_ERR("chdir failed");
        exit(EXIT_FAILURE);
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    LOG_MESSAGE_INFO("Process daemonized successfully");
}

/**
 * Cleanup daemon resources
 */
void daemon_cleanup(void) {
    LOG_MESSAGE_INFO("Daemon cleanup initiated");
    
    // Cleanup configuration
    config_cleanup();
    
    // Cleanup logger
    logger_cleanup();
}
