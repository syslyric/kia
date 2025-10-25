#ifndef KIA_SESSION_H
#define KIA_SESSION_H

#include "config.h"

/* Session types */
typedef enum {
    SESSION_X11,
    SESSION_WAYLAND
} session_type_t;

/* Session information structure */
typedef struct {
    char name[256];
    char exec[512];
    session_type_t type;
} session_info_t;

/* Session list structure */
typedef struct {
    session_info_t *sessions;
    int count;
} session_list_t;

/**
 * Discover available X11 and Wayland sessions
 * Scans /usr/share/xsessions/ and /usr/share/wayland-sessions/
 * @param list Pointer to session list to populate
 * @return KIA_SUCCESS on success, KIA_ERROR_SESSION on error
 */
int session_discover(session_list_t *list);

/**
 * Free session list resources
 * @param list Pointer to session list to free
 */
void session_list_free(session_list_t *list);

/**
 * Start a session for the specified user
 * Forks a process, drops privileges, sets environment, and executes session
 * @param session Session to start
 * @param username Username to start session for
 * @return KIA_SUCCESS on success, KIA_ERROR_SESSION on error
 */
int session_start(const session_info_t *session, const char *username);

#endif /* KIA_SESSION_H */
