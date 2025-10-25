#ifndef KIA_CONFIG_H
#define KIA_CONFIG_H

#include <stdbool.h>

/* Error codes */
#define KIA_SUCCESS          0
#define KIA_ERROR_CONFIG    -1
#define KIA_ERROR_AUTH      -2
#define KIA_ERROR_SESSION   -3
#define KIA_ERROR_SYSTEM    -4
#define KIA_ERROR_PAM       -5

/* Configuration structure */
typedef struct {
    char autologin_user[256];
    bool autologin_enabled;
    char default_session[256];
    int max_attempts;
    bool enable_logs;
    int lockout_duration;  /* seconds */
} kia_config_t;

/**
 * Load configuration from file
 * @param path Path to configuration file
 * @param config Pointer to configuration structure to populate
 * @return KIA_SUCCESS on success, KIA_ERROR_CONFIG on error
 */
int config_load(const char *path, kia_config_t *config);

/**
 * Validate configuration values
 * @param config Pointer to configuration structure to validate
 * @return KIA_SUCCESS if valid, KIA_ERROR_CONFIG if invalid
 */
int config_validate(kia_config_t *config);

/**
 * Free configuration resources
 * @param config Pointer to configuration structure to free
 */
void config_free(kia_config_t *config);

#endif /* KIA_CONFIG_H */
