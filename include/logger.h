#ifndef KIA_LOGGER_H
#define KIA_LOGGER_H

#include <stdbool.h>

/* Log levels */
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

/**
 * Initialize the logger
 * @param log_path Path to log file (e.g., /var/log/kia.log)
 * @param enabled Whether logging is enabled
 * @return KIA_SUCCESS on success, KIA_ERROR_SYSTEM on error
 */
int logger_init(const char *log_path, bool enabled);

/**
 * Log a message with the specified level
 * @param level Log level
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void logger_log(log_level_t level, const char *format, ...);

/**
 * Close the logger and cleanup resources
 */
void logger_close(void);

#endif /* KIA_LOGGER_H */
