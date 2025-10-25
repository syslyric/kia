#ifndef KIA_AUTH_H
#define KIA_AUTH_H

#include <time.h>
#include <stdbool.h>
#include "config.h"

/* Authentication state structure */
typedef struct {
    char username[256];
    int failed_attempts;
    time_t lockout_until;
} auth_state_t;

/**
 * Initialize the authentication module
 * @return KIA_SUCCESS on success, KIA_ERROR_PAM on error
 */
int auth_init(void);

/**
 * Authenticate a user with PAM
 * @param username Username to authenticate
 * @param password Password to authenticate
 * @param config Configuration containing max_attempts and lockout_duration
 * @param state Authentication state to track attempts and lockout
 * @return KIA_SUCCESS on success, KIA_ERROR_AUTH on failure
 */
int auth_authenticate(const char *username, const char *password,
                      const kia_config_t *config, auth_state_t *state);

/**
 * Check if user is currently locked out
 * @param state Authentication state to check
 * @return true if locked out, false otherwise
 */
bool auth_is_locked_out(auth_state_t *state);

/**
 * Reset failed attempt counter
 * @param state Authentication state to reset
 */
void auth_reset_attempts(auth_state_t *state);

/**
 * Cleanup authentication module resources
 */
void auth_cleanup(void);

#endif /* KIA_AUTH_H */
