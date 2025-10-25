#include "auth.h"
#include "logger.h"
#include <security/pam_appl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* PAM conversation data */
typedef struct {
    const char *password;
} pam_conv_data_t;

/* Global PAM handle */
static struct pam_handle *pam_handle = NULL;

/* TTY file descriptor for locking */
static int tty_fd = -1;

/**
 * Lock the TTY to prevent switching during authentication
 * @return KIA_SUCCESS on success, KIA_ERROR_SYSTEM on failure
 */
static int tty_lock(void) {
    /* Open current TTY */
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        logger_log(LOG_WARN, "Failed to open TTY for locking: %s", strerror(errno));
        return KIA_ERROR_SYSTEM;
    }
    
    /* Lock VT switching */
    if (ioctl(tty_fd, VT_LOCKSWITCH) < 0) {
        logger_log(LOG_WARN, "Failed to lock VT switching: %s", strerror(errno));
        close(tty_fd);
        tty_fd = -1;
        return KIA_ERROR_SYSTEM;
    }
    
    logger_log(LOG_DEBUG, "TTY locked during authentication");
    return KIA_SUCCESS;
}

/**
 * Unlock the TTY after authentication
 */
static void tty_unlock(void) {
    if (tty_fd >= 0) {
        /* Unlock VT switching */
        if (ioctl(tty_fd, VT_UNLOCKSWITCH) < 0) {
            logger_log(LOG_WARN, "Failed to unlock VT switching: %s", strerror(errno));
        }
        
        close(tty_fd);
        tty_fd = -1;
        logger_log(LOG_DEBUG, "TTY unlocked after authentication");
    }
}

/**
 * PAM conversation function
 * Handles password prompts from PAM
 */
static int pam_conversation(int num_msg, const struct pam_message **msg,
                           struct pam_response **resp, void *appdata_ptr) {
    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG) {
        return PAM_CONV_ERR;
    }

    pam_conv_data_t *conv_data = (pam_conv_data_t *)appdata_ptr;
    if (!conv_data || !conv_data->password) {
        return PAM_CONV_ERR;
    }

    /* Allocate response array */
    *resp = calloc(num_msg, sizeof(struct pam_response));
    if (!*resp) {
        return PAM_BUF_ERR;
    }

    /* Process each message */
    for (int i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                /* Provide password */
                (*resp)[i].resp = strdup(conv_data->password);
                if (!(*resp)[i].resp) {
                    /* Cleanup on allocation failure */
                    for (int j = 0; j < i; j++) {
                        free((*resp)[j].resp);
                    }
                    free(*resp);
                    *resp = NULL;
                    return PAM_BUF_ERR;
                }
                (*resp)[i].resp_retcode = 0;
                break;

            case PAM_ERROR_MSG:
            case PAM_TEXT_INFO:
                /* No response needed for info/error messages */
                (*resp)[i].resp = NULL;
                (*resp)[i].resp_retcode = 0;
                break;

            default:
                /* Unknown message type */
                for (int j = 0; j < i; j++) {
                    free((*resp)[j].resp);
                }
                free(*resp);
                *resp = NULL;
                return PAM_CONV_ERR;
        }
    }

    return PAM_SUCCESS;
}

int auth_init(void) {
    /* PAM initialization is done per-authentication in auth_authenticate() */
    logger_log(LOG_INFO, "Authentication module initialized");
    return KIA_SUCCESS;
}

int auth_authenticate(const char *username, const char *password,
                      const kia_config_t *config, auth_state_t *state) {
    int result = KIA_ERROR_AUTH;
    
    if (!username || !password || !config || !state) {
        logger_log(LOG_ERROR, "Invalid parameters to auth_authenticate");
        return KIA_ERROR_AUTH;
    }

    /* Check if user is locked out */
    if (auth_is_locked_out(state)) {
        time_t now = time(NULL);
        int remaining = (int)(state->lockout_until - now);
        logger_log(LOG_WARN, "User '%s' is locked out for %d more seconds",
                   username, remaining);
        return KIA_ERROR_AUTH;
    }

    /* Update state username if changed */
    if (strcmp(state->username, username) != 0) {
        strncpy(state->username, username, sizeof(state->username) - 1);
        state->username[sizeof(state->username) - 1] = '\0';
        state->failed_attempts = 0;
        state->lockout_until = 0;
    }

    /* Lock TTY to prevent switching during authentication */
    tty_lock();

    /* Setup PAM conversation */
    pam_conv_data_t conv_data = { .password = password };
    struct pam_conv conv = {
        .conv = pam_conversation,
        .appdata_ptr = &conv_data
    };

    /* Initialize PAM */
    int pam_result = pam_start("kia", username, &conv, &pam_handle);
    if (pam_result != PAM_SUCCESS) {
        logger_log(LOG_ERROR, "PAM initialization failed: %s",
                   pam_strerror(pam_handle, pam_result));
        tty_unlock();
        return KIA_ERROR_PAM;
    }

    /* Authenticate */
    pam_result = pam_authenticate(pam_handle, 0);

    if (pam_result == PAM_SUCCESS) {
        /* Verify account */
        pam_result = pam_acct_mgmt(pam_handle, 0);
        if (pam_result != PAM_SUCCESS) {
            logger_log(LOG_ERROR, "PAM account verification failed for user '%s': %s",
                       username, pam_strerror(pam_handle, pam_result));
            pam_end(pam_handle, pam_result);
            pam_handle = NULL;
            
            state->failed_attempts++;
            if (state->failed_attempts >= config->max_attempts) {
                state->lockout_until = time(NULL) + config->lockout_duration;
                logger_log(LOG_WARN, "User '%s' locked out after %d failed attempts",
                           username, state->failed_attempts);
            }
            
            tty_unlock();
            return KIA_ERROR_AUTH;
        }

        /* Authentication successful */
        logger_log(LOG_INFO, "User '%s' authenticated successfully", username);
        auth_reset_attempts(state);
        
        pam_end(pam_handle, PAM_SUCCESS);
        pam_handle = NULL;
        result = KIA_SUCCESS;
    } else {
        /* Authentication failed */
        state->failed_attempts++;
        logger_log(LOG_ERROR, "Authentication failed for user '%s' (attempt %d/%d): %s",
                   username, state->failed_attempts, config->max_attempts,
                   pam_strerror(pam_handle, pam_result));

        /* Check if lockout threshold reached */
        if (state->failed_attempts >= config->max_attempts) {
            state->lockout_until = time(NULL) + config->lockout_duration;
            logger_log(LOG_WARN, "User '%s' locked out after %d failed attempts",
                       username, state->failed_attempts);
        }

        pam_end(pam_handle, pam_result);
        pam_handle = NULL;
        result = KIA_ERROR_AUTH;
    }
    
    /* Unlock TTY after authentication */
    tty_unlock();
    
    return result;
}

bool auth_is_locked_out(auth_state_t *state) {
    if (!state) {
        return false;
    }

    if (state->lockout_until == 0) {
        return false;
    }

    time_t now = time(NULL);
    if (now >= state->lockout_until) {
        /* Lockout expired */
        state->lockout_until = 0;
        state->failed_attempts = 0;
        return false;
    }

    return true;
}

void auth_reset_attempts(auth_state_t *state) {
    if (state) {
        state->failed_attempts = 0;
        state->lockout_until = 0;
    }
}

void auth_cleanup(void) {
    if (pam_handle) {
        pam_end(pam_handle, PAM_SUCCESS);
        pam_handle = NULL;
    }
    
    /* Ensure TTY is unlocked */
    tty_unlock();
    
    logger_log(LOG_INFO, "Authentication module cleaned up");
}
