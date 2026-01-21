#include "Logger.h"
#include <stdarg.h>

unsigned long SystemLogger::getElapse() {
    return millis();
}

void SystemLogger::log(String msg) {
    Serial.printf("[%lu ms] %s\n", millis(), msg.c_str());
}

void SystemLogger::logf(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    Serial.printf("[%lu ms] %s\n", millis(), buf);
}

// Global helpers
void my_log(String msg) {
    SystemLogger::log(msg);
}

void my_log_f(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    SystemLogger::log(String(buf));
}

