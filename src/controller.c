#include "controller.h"
#include "config.h"
#include "logger.h"
#include "auth.h"
#include "session.h"
#include "tui.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>

/**
 * Secure memory clearing function
 * Uses volatile to prevent compiler optimization
 */
static void secure_memzero(void *ptr, size_t len) {
    volatile unsigned char *p = ptr;
    while (len--) {
        *p++ = 0;
    }
}

#define KIA_VERSION "1.0.0"
#define KIA_CONFIG_PATH "/etc/kia/config"

/* Forward declarations for state handlers */
static int handle_init(app_context_t *ctx);
static int handle_load_config(app_context_t *ctx);
static int handle_check_autologin(app_context_t *ctx);
static int handle_show_login(app_context_t *ctx);
static int handle_get_credentials(app_context_t *ctx);
static int handle_select_session(app_context_t *ctx);
static int handle_authenticate(app_context_t *ctx);
static int handle_start_session(app_context_t *ctx);

/* Helper function to get hostname */
static void get_hostname(char *hostname, size_t len) {
    /* Validate input */
    if (hostname == NULL || len == 0) {
        return;
    }
    
    if (gethostname(hostname, len) != 0) {
        /* gethostname failed - use default */
        strncpy(hostname, "localhost", len - 1);
        hostname[len - 1] = '\0';
    } else {
        /* Ensure null termination */
        hostname[len - 1] = '\0';
    }
}

/* Helper function to validate user exists */
static bool user_exists(const char *username) {
    struct passwd *pwd;
    
    /* Validate input */
    if (username == NULL || username[0] == '\0') {
        return false;
    }
    
    /* Validate username length */
    if (strlen(username) > 255) {
        return false;
    }
    
    errno = 0;
    pwd = getpwnam(username);
    
    if (pwd == NULL) {
        if (errno != 0) {
            logger_log(LOG_WARN, "Error checking user '%s': %s", username, strerror(errno));
        }
        return false;
    }
    
    return true;
}

/* Helper function to find default session index */
static int find_default_session(const session_list_t *sessions, const char *default_name) {
    /* Validate input */
    if (sessions == NULL || sessions->sessions == NULL || sessions->count <= 0) {
        return 0;
    }
    
    if (!default_name || !default_name[0]) {
        return 0;  /* Return first session if no default specified */
    }
    
    /* Search for matching session */
    for (int i = 0; i < sessions->count; i++) {
        if (sessions->sessions[i].name[0] != '\0' &&
            strcmp(sessions->sessions[i].name, default_name) == 0) {
            return i;
        }
    }
    
    logger_log(LOG_DEBUG, "Default session '%s' not found, using first session", default_name);
    return 0;  /* Return first session if default not found */
}

int controller_init(app_context_t *ctx) {
    if (!ctx) {
        return KIA_ERROR_SYSTEM;
    }
    
    /* Zero out the context */
    memset(ctx, 0, sizeof(app_context_t));
    
    /* Set initial state */
    ctx->state = STATE_INIT;
    ctx->running = true;
    ctx->selected_session = -1;
    
    /* Initialize auth state */
    memset(&ctx->auth_state, 0, sizeof(auth_state_t));
    
    return KIA_SUCCESS;
}

int controller_run(app_context_t *ctx) {
    if (!ctx) {
        return KIA_ERROR_SYSTEM;
    }
    
    int result = KIA_SUCCESS;
    
    /* Main event loop */
    while (ctx->running && ctx->state != STATE_EXIT) {
        switch (ctx->state) {
            case STATE_INIT:
                result = handle_init(ctx);
                break;
                
            case STATE_LOAD_CONFIG:
                result = handle_load_config(ctx);
                break;
                
            case STATE_CHECK_AUTOLOGIN:
                result = handle_check_autologin(ctx);
                break;
                
            case STATE_SHOW_LOGIN:
                result = handle_show_login(ctx);
                break;
                
            case STATE_GET_CREDENTIALS:
                result = handle_get_credentials(ctx);
                break;
                
            case STATE_SELECT_SESSION:
                result = handle_select_session(ctx);
                break;
                
            case STATE_AUTHENTICATE:
                result = handle_authenticate(ctx);
                break;
                
            case STATE_START_SESSION:
                result = handle_start_session(ctx);
                break;
                
            case STATE_EXIT:
                /* Exit state - will break loop */
                break;
                
            default:
                logger_log(LOG_ERROR, "Unknown state: %d", ctx->state);
                ctx->state = STATE_EXIT;
                result = KIA_ERROR_SYSTEM;
                break;
        }
        
        /* If any state handler fails critically, exit */
        if (result != KIA_SUCCESS && ctx->state == STATE_EXIT) {
            break;
        }
    }
    
    return result;
}

void controller_cleanup(app_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    /* Securely clear sensitive data (password) */
    secure_memzero(ctx->password, sizeof(ctx->password));
    
    /* Free configuration */
    config_free(&ctx->config);
    
    /* Free session list */
    session_list_free(&ctx->sessions);
    
    /* Cleanup authentication module */
    auth_cleanup();
    
    /* Cleanup TUI */
    tui_cleanup();
    
    /* Close logger */
    logger_close();
}

/* State handler implementations */

static int handle_init(app_context_t *ctx) {
    logger_log(LOG_INFO, "Kia display manager started (version %s)", KIA_VERSION);
    
    /* Initialize authentication module */
    int result = auth_init();
    if (result != KIA_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize authentication module");
        ctx->state = STATE_EXIT;
        return result;
    }
    
    /* Initialize TUI */
    result = tui_init();
    if (result != KIA_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize TUI");
        ctx->state = STATE_EXIT;
        return result;
    }
    
    /* Transition to load config */
    ctx->state = STATE_LOAD_CONFIG;
    return KIA_SUCCESS;
}

static int handle_load_config(app_context_t *ctx) {
    int result = config_load(KIA_CONFIG_PATH, &ctx->config);
    
    if (result != KIA_SUCCESS) {
        logger_log(LOG_WARN, "Failed to load config from %s, using defaults", KIA_CONFIG_PATH);
        /* Continue with defaults - not a critical error */
    }
    
    /* Validate configuration */
    result = config_validate(&ctx->config);
    if (result != KIA_SUCCESS) {
        logger_log(LOG_WARN, "Configuration validation failed, using defaults");
    }
    
    /* Discover available sessions */
    result = session_discover(&ctx->sessions);
    if (result != KIA_SUCCESS || ctx->sessions.count == 0) {
        logger_log(LOG_ERROR, "No sessions found");
        tui_show_error("No sessions available. Please install a desktop environment.");
        ctx->state = STATE_EXIT;
        return KIA_ERROR_SESSION;
    }
    
    logger_log(LOG_INFO, "Discovered %d session(s)", ctx->sessions.count);
    
    /* Transition to autologin check */
    ctx->state = STATE_CHECK_AUTOLOGIN;
    return KIA_SUCCESS;
}

static int handle_check_autologin(app_context_t *ctx) {
    /* Check if autologin is enabled */
    if (ctx->config.autologin_enabled && ctx->config.autologin_user[0] != '\0') {
        /* Validate username length */
        size_t username_len = strlen(ctx->config.autologin_user);
        if (username_len == 0 || username_len >= sizeof(ctx->username)) {
            logger_log(LOG_ERROR, "Invalid autologin username length: %zu", username_len);
            tui_show_error("Invalid autologin configuration. Falling back to manual login.");
            ctx->state = STATE_SHOW_LOGIN;
            return KIA_SUCCESS;
        }
        
        /* Validate that the autologin user exists */
        if (!user_exists(ctx->config.autologin_user)) {
            logger_log(LOG_ERROR, "Autologin user '%s' does not exist", ctx->config.autologin_user);
            tui_show_error("Autologin user not found. Falling back to manual login.");
            ctx->state = STATE_SHOW_LOGIN;
            return KIA_SUCCESS;
        }
        
        /* Set username for autologin with bounds checking */
        strncpy(ctx->username, ctx->config.autologin_user, sizeof(ctx->username) - 1);
        ctx->username[sizeof(ctx->username) - 1] = '\0';
        
        /* Find default session */
        ctx->selected_session = find_default_session(&ctx->sessions, ctx->config.default_session);
        
        /* Validate session index */
        if (ctx->selected_session < 0 || ctx->selected_session >= ctx->sessions.count) {
            logger_log(LOG_ERROR, "Invalid session index for autologin: %d", ctx->selected_session);
            tui_show_error("Invalid session configuration. Falling back to manual login.");
            ctx->state = STATE_SHOW_LOGIN;
            return KIA_SUCCESS;
        }
        
        logger_log(LOG_INFO, "Autologin enabled for user '%s' with session '%s'", 
                   ctx->username, ctx->sessions.sessions[ctx->selected_session].name);
        
        /* Skip to session start */
        ctx->state = STATE_START_SESSION;
    } else {
        /* No autologin, show login screen */
        ctx->state = STATE_SHOW_LOGIN;
    }
    
    return KIA_SUCCESS;
}

static int handle_show_login(app_context_t *ctx) {
    char hostname[256];
    get_hostname(hostname, sizeof(hostname));
    
    /* Draw the login screen */
    tui_draw_login_screen(hostname, KIA_VERSION);
    
    /* Transition to get credentials */
    ctx->state = STATE_GET_CREDENTIALS;
    return KIA_SUCCESS;
}

static int handle_get_credentials(app_context_t *ctx) {
    /* Get credentials from user */
    int result = tui_get_credentials(ctx->username, sizeof(ctx->username),
                                     ctx->password, sizeof(ctx->password));
    
    if (result != KIA_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to get credentials from TUI: %d", result);
        tui_show_error("Failed to read credentials. Please try again.");
        ctx->state = STATE_SHOW_LOGIN;
        return result;
    }
    
    /* Validate username is not empty */
    if (ctx->username[0] == '\0') {
        logger_log(LOG_WARN, "Empty username provided");
        tui_show_error("Username cannot be empty.");
        ctx->state = STATE_SHOW_LOGIN;
        return KIA_SUCCESS;
    }
    
    /* Validate username length */
    if (strlen(ctx->username) > 255) {
        logger_log(LOG_WARN, "Username too long: %zu characters", strlen(ctx->username));
        tui_show_error("Username too long.");
        /* Securely clear credentials */
        memset(ctx->username, 0, sizeof(ctx->username));
        secure_memzero(ctx->password, sizeof(ctx->password));
        ctx->state = STATE_SHOW_LOGIN;
        return KIA_SUCCESS;
    }
    
    /* Validate password is not empty */
    if (ctx->password[0] == '\0') {
        logger_log(LOG_WARN, "Empty password provided for user '%s'", ctx->username);
        tui_show_error("Password cannot be empty.");
        ctx->state = STATE_SHOW_LOGIN;
        return KIA_SUCCESS;
    }
    
    /* Transition to session selection */
    ctx->state = STATE_SELECT_SESSION;
    return KIA_SUCCESS;
}

static int handle_select_session(app_context_t *ctx) {
    /* Find default session index */
    int default_idx = find_default_session(&ctx->sessions, ctx->config.default_session);
    
    /* Let user select session */
    ctx->selected_session = tui_select_session(&ctx->sessions, default_idx);
    
    if (ctx->selected_session < 0 || ctx->selected_session >= ctx->sessions.count) {
        logger_log(LOG_ERROR, "Invalid session selection: %d", ctx->selected_session);
        tui_show_error("Invalid session selection.");
        ctx->state = STATE_SHOW_LOGIN;
        return KIA_ERROR_SESSION;
    }
    
    logger_log(LOG_INFO, "User '%s' selected session: %s", 
               ctx->username, ctx->sessions.sessions[ctx->selected_session].name);
    
    /* Transition to authentication */
    ctx->state = STATE_AUTHENTICATE;
    return KIA_SUCCESS;
}

static int handle_authenticate(app_context_t *ctx) {
    /* Check if user is locked out */
    if (auth_is_locked_out(&ctx->auth_state)) {
        logger_log(LOG_WARN, "User '%s' is locked out", ctx->username);
        tui_show_error("Too many failed attempts. Please wait before trying again.");
        
        /* Securely clear password */
        secure_memzero(ctx->password, sizeof(ctx->password));
        
        ctx->state = STATE_SHOW_LOGIN;
        return KIA_SUCCESS;
    }
    
    /* Attempt authentication */
    int result = auth_authenticate(ctx->username, ctx->password, 
                                   &ctx->config, &ctx->auth_state);
    
    /* Securely clear password from memory immediately after authentication */
    secure_memzero(ctx->password, sizeof(ctx->password));
    
    if (result == KIA_SUCCESS) {
        /* Authentication successful */
        logger_log(LOG_INFO, "User '%s' authenticated successfully", ctx->username);
        auth_reset_attempts(&ctx->auth_state);
        
        /* Transition to start session */
        ctx->state = STATE_START_SESSION;
    } else {
        /* Authentication failed */
        logger_log(LOG_ERROR, "Authentication failed for user '%s' (attempt %d/%d)",
                   ctx->username, ctx->auth_state.failed_attempts, ctx->config.max_attempts);
        
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Authentication failed. Attempt %d of %d.",
                ctx->auth_state.failed_attempts, ctx->config.max_attempts);
        tui_show_error(error_msg);
        
        /* Return to login screen */
        ctx->state = STATE_SHOW_LOGIN;
    }
    
    return KIA_SUCCESS;
}

static int handle_start_session(app_context_t *ctx) {
    /* Validate session selection */
    if (ctx->selected_session < 0 || ctx->selected_session >= ctx->sessions.count) {
        logger_log(LOG_ERROR, "Invalid session index: %d", ctx->selected_session);
        tui_show_error("Invalid session. Please try again.");
        ctx->state = STATE_SHOW_LOGIN;
        return KIA_ERROR_SESSION;
    }
    
    session_info_t *session = &ctx->sessions.sessions[ctx->selected_session];
    
    logger_log(LOG_INFO, "Starting %s session '%s' for user '%s'",
               session->type == SESSION_X11 ? "X11" : "Wayland",
               session->name, ctx->username);
    
    /* Show message to user */
    tui_show_message("Starting session...");
    
    /* Start the session */
    int result = session_start(session, ctx->username);
    
    if (result != KIA_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to start session for user '%s'", ctx->username);
        tui_show_error("Failed to start session. Please try again.");
        ctx->state = STATE_SHOW_LOGIN;
        return result;
    }
    
    /* Session started successfully - exit the display manager */
    logger_log(LOG_INFO, "Session started successfully, exiting display manager");
    ctx->state = STATE_EXIT;
    
    return KIA_SUCCESS;
}
