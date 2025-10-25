#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* Logger state */
static FILE *log_file = NULL;
static bool logging_enabled = false;
static log_level_t min_log_level = LOG_DEBUG;

/* Log level strings */
static const char *log_level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

/**
 * Get current timestamp in ISO 8601 format
 * @param buffer Buffer to store timestamp (must be at least 32 bytes)
 * @param size Size of buffer
 */
static void get_iso8601_timestamp(char *buffer, size_t size) {
    time_t now;
    struct tm *tm_info;
    
    /* Validate input */
    if (buffer == NULL || size == 0) {
        return;
    }
    
    /* Get current time */
    if (time(&now) == (time_t)-1) {
        snprintf(buffer, size, "UNKNOWN");
        return;
    }
    
    tm_info = gmtime(&now);
    
    if (tm_info != NULL) {
        if (strftime(buffer, size, "%Y-%m-%dT%H:%M:%SZ", tm_info) == 0) {
            /* strftime failed - buffer too small */
            snprintf(buffer, size, "UNKNOWN");
        }
    } else {
        snprintf(buffer, size, "UNKNOWN");
    }
}

int logger_init(const char *log_path, bool enabled) {
    if (log_path == NULL) {
        return KIA_ERROR_SYSTEM;
    }
    
    logging_enabled = enabled;
    
    /* If logging is disabled, don't open the file */
    if (!enabled) {
        return KIA_SUCCESS;
    }
    
    /* Open log file in append mode */
    log_file = fopen(log_path, "a");
    if (log_file == NULL) {
        /* Failed to open log file - disable logging but don't fail */
        logging_enabled = false;
        return KIA_ERROR_SYSTEM;
    }
    
    /* Set proper permissions (0640) */
    if (chmod(log_path, 0640) != 0) {
        /* Permission setting failed, but continue anyway */
        /* This is not critical enough to fail initialization */
    }
    
    /* Make output unbuffered for immediate writes */
    setbuf(log_file, NULL);
    
    return KIA_SUCCESS;
}

void logger_log(log_level_t level, const char *format, ...) {
    char timestamp[32];
    char message[1024];
    va_list args;
    
    /* Validate input parameters */
    if (format == NULL) {
        return;
    }
    
    /* Check if logging is enabled */
    if (!logging_enabled || log_file == NULL) {
        return;
    }
    
    /* Check log level filtering */
    if (level < min_log_level) {
        return;
    }
    
    /* Validate log level */
    if (level < LOG_DEBUG || level > LOG_ERROR) {
        return;
    }
    
    /* Get timestamp */
    get_iso8601_timestamp(timestamp, sizeof(timestamp));
    
    /* Format the message */
    va_start(args, format);
    int result = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Check for formatting errors */
    if (result < 0) {
        /* Formatting failed - disable logging */
        logging_enabled = false;
        return;
    }
    
    /* Write log entry */
    if (fprintf(log_file, "%s [%s] %s\n", 
                timestamp, log_level_strings[level], message) < 0) {
        /* Write failed - disable logging to prevent further errors */
        logging_enabled = false;
    }
    
    /* Flush to ensure immediate write */
    if (fflush(log_file) != 0) {
        /* Flush failed - disable logging */
        logging_enabled = false;
    }
}

void logger_close(void) {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
    logging_enabled = false;
}
