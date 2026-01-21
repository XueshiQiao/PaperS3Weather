#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

class SystemLogger {
public:
    // Returns milliseconds elapsed since boot
    static unsigned long getElapse();

    // Logs a message with a timestamp prefix (adds newline automatically)
    static void log(String msg);

    // Logs a formatted message with timestamp (printf style)
    static void logf(const char* format, ...);
};

// Global helper function for easier access
void my_log(String msg);
void my_log_f(const char* format, ...);

#endif // LOGGER_H
