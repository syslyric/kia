#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Default configuration values */
#define DEFAULT_AUTOLOGIN_ENABLED false
#define DEFAULT_AUTOLOGIN_USER ""
#define DEFAULT_SESSION "xfce"
#define DEFAULT_MAX_ATTEMPTS 3
#define DEFAULT_ENABLE_LOGS true
#define DEFAULT_LOCKOUT_DURATION 60

/* Configuration constraints */
#define MIN_MAX_ATTEMPTS 1
#define MAX_MAX_ATTEMPTS 10

/**
 * Initialize configuration with default values
 */
static void config_set_defaults(kia_config_t *config) {
    config->autologin_enabled = DEFAULT_AUTOLOGIN_ENABLED;
    strncpy(config->autologin_user, DEFAULT_AUTOLOGIN_USER, sizeof(config->autologin_user) - 1);
    config->autologin_user[sizeof(config->autologin_user) - 1] = '\0';
    strncpy(config->default_session, DEFAULT_SESSION, sizeof(config->default_session) - 1);
    config->default_session[sizeof(config->default_session) - 1] = '\0';
    config->max_attempts = DEFAULT_MAX_ATTEMPTS;
    config->enable_logs = DEFAULT_ENABLE_LOGS;
    config->lockout_duration = DEFAULT_LOCKOUT_DURATION;
}

/**
 * Trim whitespace from both ends of a string
 */
static char *trim_whitespace(char *str) {
    char *end;
    
    /* Validate input */
    if (str == NULL) {
        return NULL;
    }
    
    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str;
    
    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    /* Write new null terminator */
    end[1] = '\0';
    
    return str;
}

/**
 * Parse boolean value from string
 */
static bool parse_bool(const char *value) {
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || 
        strcmp(value, "yes") == 0 || strcmp(value, "on") == 0) {
        return true;
    }
    return false;
}

/**
 * Parse a single configuration line
 */
static int parse_config_line(char *line, kia_config_t *config) {
    char *key, *value, *equals;
    
    /* Validate input */
    if (line == NULL || config == NULL) {
        return KIA_ERROR_CONFIG;
    }
    
    /* Skip empty lines and comments */
    if (line[0] == '\0' || line[0] == '#') {
        return KIA_SUCCESS;
    }
    
    /* Find the equals sign */
    equals = strchr(line, '=');
    if (equals == NULL) {
        /* Invalid syntax - no equals sign */
        return KIA_ERROR_CONFIG;
    }
    
    /* Split into key and value */
    *equals = '\0';
    key = trim_whitespace(line);
    value = trim_whitespace(equals + 1);
    
    /* Validate key and value are not empty */
    if (key[0] == '\0') {
        return KIA_ERROR_CONFIG;
    }
    
    /* Parse known configuration keys */
    if (strcmp(key, "autologin_enabled") == 0) {
        config->autologin_enabled = parse_bool(value);
    } else if (strcmp(key, "autologin_user") == 0) {
        /* Validate username length and characters */
        size_t value_len = strlen(value);
        if (value_len >= sizeof(config->autologin_user)) {
            return KIA_ERROR_CONFIG;
        }
        strncpy(config->autologin_user, value, sizeof(config->autologin_user) - 1);
        config->autologin_user[sizeof(config->autologin_user) - 1] = '\0';
    } else if (strcmp(key, "default_session") == 0) {
        /* Validate session name length */
        size_t value_len = strlen(value);
        if (value_len >= sizeof(config->default_session)) {
            return KIA_ERROR_CONFIG;
        }
        strncpy(config->default_session, value, sizeof(config->default_session) - 1);
        config->default_session[sizeof(config->default_session) - 1] = '\0';
    } else if (strcmp(key, "max_attempts") == 0) {
        /* Validate numeric value */
        if (value[0] == '\0') {
            return KIA_ERROR_CONFIG;
        }
        int attempts = atoi(value);
        if (attempts < MIN_MAX_ATTEMPTS || attempts > MAX_MAX_ATTEMPTS) {
            return KIA_ERROR_CONFIG;
        }
        config->max_attempts = attempts;
    } else if (strcmp(key, "enable_logs") == 0) {
        config->enable_logs = parse_bool(value);
    } else if (strcmp(key, "lockout_duration") == 0) {
        /* Validate numeric value */
        if (value[0] == '\0') {
            return KIA_ERROR_CONFIG;
        }
        int duration = atoi(value);
        if (duration < 0 || duration > 3600) {  /* Max 1 hour */
            return KIA_ERROR_CONFIG;
        }
        config->lockout_duration = duration;
    }
    /* Unknown keys are silently ignored */
    
    return KIA_SUCCESS;
}

int config_load(const char *path, kia_config_t *config) {
    FILE *file;
    char line[1024];
    int line_num = 0;
    int has_error = 0;
    
    /* Validate input parameters */
    if (config == NULL) {
        return KIA_ERROR_CONFIG;
    }
    
    if (path == NULL || path[0] == '\0') {
        /* Invalid path - use defaults */
        config_set_defaults(config);
        return KIA_ERROR_CONFIG;
    }
    
    /* Set default values first */
    config_set_defaults(config);
    
    /* Try to open the configuration file */
    file = fopen(path, "r");
    if (file == NULL) {
        /* File doesn't exist - use defaults */
        return KIA_SUCCESS;
    }
    
    /* Read and parse each line */
    while (fgets(line, sizeof(line), file) != NULL) {
        line_num++;
        
        /* Check for line truncation */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(file)) {
            /* Line was truncated - skip rest of line */
            int ch;
            while ((ch = fgetc(file)) != EOF && ch != '\n') {
                /* Skip to end of line */
            }
            has_error = 1;
            continue;
        }
        
        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';
        
        /* Trim whitespace */
        char *trimmed = trim_whitespace(line);
        
        /* Parse the line */
        if (parse_config_line(trimmed, config) != KIA_SUCCESS) {
            /* Invalid syntax on this line */
            has_error = 1;
        }
    }
    
    /* Check for read errors */
    if (ferror(file)) {
        has_error = 1;
    }
    
    fclose(file);
    
    /* Validate the configuration */
    if (config_validate(config) != KIA_SUCCESS) {
        /* Invalid configuration values - reset to defaults */
        config_set_defaults(config);
        return KIA_ERROR_CONFIG;
    }
    
    return has_error ? KIA_ERROR_CONFIG : KIA_SUCCESS;
}

int config_validate(kia_config_t *config) {
    if (config == NULL) {
        return KIA_ERROR_CONFIG;
    }
    
    /* Validate max_attempts is in range [1, 10] */
    if (config->max_attempts < MIN_MAX_ATTEMPTS || 
        config->max_attempts > MAX_MAX_ATTEMPTS) {
        return KIA_ERROR_CONFIG;
    }
    
    /* Validate lockout_duration is positive */
    if (config->lockout_duration < 0) {
        return KIA_ERROR_CONFIG;
    }
    
    return KIA_SUCCESS;
}

void config_free(kia_config_t *config) {
    /* Currently no dynamic memory to free */
    /* This function is provided for future extensibility */
    (void)config;
}
