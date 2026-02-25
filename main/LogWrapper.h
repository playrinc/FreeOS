//
//  LogWrapper.h
//  
//
//  Created by Chris Galzerano on 2/9/26.
//

#ifndef LOG_WRAPPER_H
#define LOG_WRAPPER_H

// ==========================================
// ESP32 IMPLEMENTATION (Passthrough)
// ==========================================
#ifdef ESP_PLATFORM

    #include "esp_log.h"

    // Direct mapping to ESP-IDF logging
    #define FreeOSLogI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
    #define FreeOSLogE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
    #define FreeOSLogW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
    #define FreeOSLogD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
    #define FreeOSLogV(tag, format, ...) ESP_LOGV(tag, format, ##__VA_ARGS__)

// ==========================================
// LINUX IMPLEMENTATION (Printf + Colors)
// ==========================================
#else

    #include <stdio.h>
    #include <time.h>
    #include <sys/time.h>

    // ANSI Color Codes
    #define LOG_COLOR_RESET  "\033[0m"
    #define LOG_COLOR_RED    "\033[0;31m"
    #define LOG_COLOR_GREEN  "\033[0;32m"
    #define LOG_COLOR_YELLOW "\033[0;33m"
    #define LOG_COLOR_BLUE   "\033[0;34m"
    #define LOG_COLOR_CYAN   "\033[0;36m"

    // Helper to get milliseconds since boot (mimics ESP timestamp)
    static inline unsigned long _linux_get_time_ms(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    }

    // Generic Logger Macro
    // Output Format: I (1234) TAG: Message
    #define _LINUX_LOG(level_char, color, tag, format, ...) \
        do { \
            unsigned long time = _linux_get_time_ms(); \
            printf("%s%c (%lu) %s: " format "%s\n", \
                   color, level_char, time, tag, ##__VA_ARGS__, LOG_COLOR_RESET); \
        } while(0)

    // The Replacement Macros
    #define FreeOSLogI(tag, format, ...) _LINUX_LOG('I', LOG_COLOR_GREEN,  tag, format, ##__VA_ARGS__)
    #define FreeOSLogE(tag, format, ...) _LINUX_LOG('E', LOG_COLOR_RED,    tag, format, ##__VA_ARGS__)
    #define FreeOSLogW(tag, format, ...) _LINUX_LOG('W', LOG_COLOR_YELLOW, tag, format, ##__VA_ARGS__)
    #define FreeOSLogD(tag, format, ...) _LINUX_LOG('D', LOG_COLOR_CYAN,   tag, format, ##__VA_ARGS__)
    #define FreeOSLogV(tag, format, ...) _LINUX_LOG('V', LOG_COLOR_BLUE,   tag, format, ##__VA_ARGS__)

#endif // ESP_PLATFORM

#endif // LOG_WRAPPER_H
