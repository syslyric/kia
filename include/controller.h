#ifndef KIA_CONTROLLER_H
#define KIA_CONTROLLER_H

#include "config.h"
#include "auth.h"
#include "session.h"

/* Application states */
typedef enum {
    STATE_INIT,
    STATE_LOAD_CONFIG,
    STATE_CHECK_AUTOLOGIN,
    STATE_SHOW_LOGIN,
    STATE_GET_CREDENTIALS,
    STATE_SELECT_SESSION,
    STATE_AUTHENTICATE,
    STATE_START_SESSION,
    STATE_EXIT
} app_state_t;

/* Application context structure */
typedef struct {
    app_state_t state;
    kia_config_t config;
    auth_state_t auth_state;
    session_list_t sessions;
    char username[256];
    char password[256];
    int selected_session;
    bool running;
} app_context_t;

/**
 * Initialize the application controller
 * Sets up initial state and zeroes out context
 * @param ctx Pointer to application context to initialize
 * @return KIA_SUCCESS on success, error code on failure
 */
int controller_init(app_context_t *ctx);

/**
 * Main event loop processing state transitions
 * Implements the state machine for login flow
 * @param ctx Pointer to application context
 * @return KIA_SUCCESS on success, error code on failure
 */
int controller_run(app_context_t *ctx);

/**
 * Cleanup all resources allocated by the controller
 * Frees config, sessions, and clears sensitive data
 * @param ctx Pointer to application context to cleanup
 */
void controller_cleanup(app_context_t *ctx);

#endif /* KIA_CONTROLLER_H */
