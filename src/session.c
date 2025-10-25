#define _GNU_SOURCE
#include "session.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>

#define X11_SESSION_DIR "/usr/share/xsessions"
#define WAYLAND_SESSION_DIR "/usr/share/wayland-sessions"
#define MAX_LINE_LENGTH 1024

/**
 * Parse a .desktop file to extract Name and Exec fields
 */
static int parse_desktop_file(const char *filepath, session_info_t *session, session_type_t type) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    int found_name = 0, found_exec = 0;
    
    /* Validate input parameters */
    if (filepath == NULL || session == NULL) {
        logger_log(LOG_ERROR, "Invalid parameters to parse_desktop_file");
        return KIA_ERROR_SESSION;
    }
    
    /* Validate filepath length */
    if (strlen(filepath) == 0) {
        logger_log(LOG_ERROR, "Empty filepath provided");
        return KIA_ERROR_SESSION;
    }
    
    fp = fopen(filepath, "r");
    if (!fp) {
        logger_log(LOG_WARN, "Failed to open desktop file: %s (%s)", filepath, strerror(errno));
        return KIA_ERROR_SESSION;
    }

    /* Initialize session structure */
    memset(session->name, 0, sizeof(session->name));
    memset(session->exec, 0, sizeof(session->exec));

    while (fgets(line, sizeof(line), fp)) {
        /* Check for line truncation */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(fp)) {
            /* Line was truncated - skip rest of line */
            int ch;
            while ((ch = fgetc(fp)) != EOF && ch != '\n') {
                /* Skip to end of line */
            }
            continue;
        }
        
        /* Remove trailing newline */
        line[strcspn(line, "\n")] = '\0';

        /* Parse Name= field */
        if (strncmp(line, "Name=", 5) == 0 && !found_name) {
            size_t name_len = strlen(line + 5);
            if (name_len > 0 && name_len < sizeof(session->name)) {
                strncpy(session->name, line + 5, sizeof(session->name) - 1);
                session->name[sizeof(session->name) - 1] = '\0';
                found_name = 1;
            }
        }
        /* Parse Exec= field */
        else if (strncmp(line, "Exec=", 5) == 0 && !found_exec) {
            size_t exec_len = strlen(line + 5);
            if (exec_len > 0 && exec_len < sizeof(session->exec)) {
                strncpy(session->exec, line + 5, sizeof(session->exec) - 1);
                session->exec[sizeof(session->exec) - 1] = '\0';
                found_exec = 1;
            }
        }

        if (found_name && found_exec) {
            break;
        }
    }
    
    /* Check for read errors */
    if (ferror(fp)) {
        logger_log(LOG_ERROR, "Error reading desktop file: %s", filepath);
        fclose(fp);
        return KIA_ERROR_SESSION;
    }

    fclose(fp);

    if (!found_name || !found_exec) {
        logger_log(LOG_WARN, "Incomplete desktop file: %s (name=%d, exec=%d)", 
                   filepath, found_name, found_exec);
        return KIA_ERROR_SESSION;
    }

    session->type = type;
    return KIA_SUCCESS;
}

/**
 * Scan a directory for .desktop files and add them to the session list
 */
static int scan_session_directory(const char *dir_path, session_type_t type, 
                                   session_info_t **sessions, int *count) {
    DIR *dir;
    struct dirent *entry;
    
    /* Validate input parameters */
    if (dir_path == NULL || sessions == NULL || count == NULL) {
        logger_log(LOG_ERROR, "Invalid parameters to scan_session_directory");
        return KIA_ERROR_SESSION;
    }
    
    /* Validate count is not negative */
    if (*count < 0) {
        logger_log(LOG_ERROR, "Invalid session count: %d", *count);
        return KIA_ERROR_SESSION;
    }
    
    dir = opendir(dir_path);
    if (!dir) {
        logger_log(LOG_DEBUG, "Session directory not found: %s (%s)", dir_path, strerror(errno));
        return KIA_SUCCESS;  /* Not an error if directory doesn't exist */
    }

    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip non-.desktop files */
        if (strstr(entry->d_name, ".desktop") == NULL) {
            continue;
        }
        
        /* Validate filename length */
        size_t name_len = strlen(entry->d_name);
        if (name_len == 0 || name_len > 255) {
            logger_log(LOG_WARN, "Invalid filename length: %zu", name_len);
            continue;
        }

        /* Build full path with bounds checking */
        char filepath[512];
        int path_len = snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
        if (path_len < 0 || (size_t)path_len >= sizeof(filepath)) {
            logger_log(LOG_WARN, "Path too long for: %s/%s", dir_path, entry->d_name);
            continue;
        }

        /* Allocate space for new session */
        session_info_t *new_sessions = realloc(*sessions, 
                                               sizeof(session_info_t) * (*count + 1));
        if (!new_sessions) {
            logger_log(LOG_ERROR, "Failed to allocate memory for session list: %s", strerror(errno));
            closedir(dir);
            return KIA_ERROR_SESSION;
        }
        *sessions = new_sessions;

        /* Parse desktop file */
        if (parse_desktop_file(filepath, &(*sessions)[*count], type) == KIA_SUCCESS) {
            logger_log(LOG_DEBUG, "Discovered session: %s (%s)", 
                      (*sessions)[*count].name, 
                      type == SESSION_X11 ? "X11" : "Wayland");
            (*count)++;
        }
        
        errno = 0;
    }
    
    /* Check for readdir errors */
    if (errno != 0) {
        logger_log(LOG_ERROR, "Error reading directory %s: %s", dir_path, strerror(errno));
        closedir(dir);
        return KIA_ERROR_SESSION;
    }

    if (closedir(dir) != 0) {
        logger_log(LOG_WARN, "Failed to close directory %s: %s", dir_path, strerror(errno));
    }
    
    return KIA_SUCCESS;
}

int session_discover(session_list_t *list) {
    if (!list) {
        return KIA_ERROR_SESSION;
    }

    list->sessions = NULL;
    list->count = 0;

    /* Scan X11 sessions */
    if (scan_session_directory(X11_SESSION_DIR, SESSION_X11, 
                               &list->sessions, &list->count) != KIA_SUCCESS) {
        session_list_free(list);
        return KIA_ERROR_SESSION;
    }

    /* Scan Wayland sessions */
    if (scan_session_directory(WAYLAND_SESSION_DIR, SESSION_WAYLAND, 
                               &list->sessions, &list->count) != KIA_SUCCESS) {
        session_list_free(list);
        return KIA_ERROR_SESSION;
    }

    if (list->count == 0) {
        logger_log(LOG_ERROR, "No sessions discovered");
        return KIA_ERROR_SESSION;
    }

    logger_log(LOG_INFO, "Discovered %d session(s)", list->count);
    return KIA_SUCCESS;
}

void session_list_free(session_list_t *list) {
    if (list && list->sessions) {
        free(list->sessions);
        list->sessions = NULL;
        list->count = 0;
    }
}

int session_start(const session_info_t *session, const char *username) {
    struct passwd *pw;
    pid_t pid;
    int status;
    
    /* Validate input parameters */
    if (!session || !username) {
        logger_log(LOG_ERROR, "Invalid session or username");
        return KIA_ERROR_SESSION;
    }
    
    /* Validate username is not empty */
    if (username[0] == '\0') {
        logger_log(LOG_ERROR, "Empty username provided");
        return KIA_ERROR_SESSION;
    }
    
    /* Validate session name and exec are not empty */
    if (session->name[0] == '\0' || session->exec[0] == '\0') {
        logger_log(LOG_ERROR, "Invalid session: empty name or exec");
        return KIA_ERROR_SESSION;
    }

    /* Get user information */
    errno = 0;
    pw = getpwnam(username);
    if (!pw) {
        if (errno != 0) {
            logger_log(LOG_ERROR, "Failed to get user info for '%s': %s", username, strerror(errno));
        } else {
            logger_log(LOG_ERROR, "User not found: %s", username);
        }
        return KIA_ERROR_SESSION;
    }
    
    /* Validate user information */
    if (pw->pw_dir == NULL || pw->pw_dir[0] == '\0') {
        logger_log(LOG_ERROR, "User '%s' has no home directory", username);
        return KIA_ERROR_SESSION;
    }
    
    if (pw->pw_shell == NULL || pw->pw_shell[0] == '\0') {
        logger_log(LOG_WARN, "User '%s' has no shell, using /bin/sh", username);
        pw->pw_shell = "/bin/sh";
    }

    logger_log(LOG_INFO, "Starting %s session '%s' for user '%s'",
              session->type == SESSION_X11 ? "X11" : "Wayland",
              session->name, username);

    pid = fork();
    if (pid < 0) {
        logger_log(LOG_ERROR, "Failed to fork: %s", strerror(errno));
        return KIA_ERROR_SESSION;
    }

    if (pid == 0) {
        /* Child process */
        
        /* Set environment variables with error checking */
        if (setenv("HOME", pw->pw_dir, 1) != 0) {
            logger_log(LOG_ERROR, "Failed to set HOME: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        if (setenv("USER", username, 1) != 0) {
            logger_log(LOG_ERROR, "Failed to set USER: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        if (setenv("LOGNAME", username, 1) != 0) {
            logger_log(LOG_ERROR, "Failed to set LOGNAME: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        if (setenv("SHELL", pw->pw_shell, 1) != 0) {
            logger_log(LOG_ERROR, "Failed to set SHELL: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        if (session->type == SESSION_X11) {
            if (setenv("XDG_SESSION_TYPE", "x11", 1) != 0 ||
                setenv("DISPLAY", ":0", 1) != 0) {
                logger_log(LOG_ERROR, "Failed to set X11 environment: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else {
            if (setenv("XDG_SESSION_TYPE", "wayland", 1) != 0 ||
                setenv("WAYLAND_DISPLAY", "wayland-0", 1) != 0) {
                logger_log(LOG_ERROR, "Failed to set Wayland environment: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        /* Change to user's home directory */
        if (chdir(pw->pw_dir) != 0) {
            logger_log(LOG_ERROR, "Failed to change to home directory '%s': %s", 
                      pw->pw_dir, strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* Drop privileges - order matters! */
        if (setgid(pw->pw_gid) != 0) {
            logger_log(LOG_ERROR, "Failed to setgid(%d): %s", pw->pw_gid, strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (setuid(pw->pw_uid) != 0) {
            logger_log(LOG_ERROR, "Failed to setuid(%d): %s", pw->pw_uid, strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* Verify privilege drop - critical security check */
        if (getuid() != pw->pw_uid || geteuid() != pw->pw_uid ||
            getgid() != pw->pw_gid || getegid() != pw->pw_gid) {
            logger_log(LOG_ERROR, "Failed to drop privileges properly (uid=%d/%d, gid=%d/%d)",
                      getuid(), geteuid(), getgid(), getegid());
            exit(EXIT_FAILURE);
        }

        /* Execute session */
        if (session->type == SESSION_X11) {
            /* Try startx first, fall back to direct execution */
            execlp("startx", "startx", session->exec, NULL);
            /* If startx fails, try direct execution */
            execlp("/bin/sh", "sh", "-c", session->exec, NULL);
        } else {
            /* Wayland: direct compositor execution */
            execlp("/bin/sh", "sh", "-c", session->exec, NULL);
        }

        /* If we get here, exec failed */
        logger_log(LOG_ERROR, "Failed to execute session '%s': %s", session->exec, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Parent process */
    logger_log(LOG_INFO, "Session started with PID %d", pid);
    
    /* Wait for child to prevent zombie processes */
    pid_t wait_result = waitpid(pid, &status, 0);
    if (wait_result < 0) {
        logger_log(LOG_ERROR, "Failed to wait for child process: %s", strerror(errno));
        return KIA_ERROR_SESSION;
    }
    
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        logger_log(LOG_INFO, "Session exited with status %d", exit_status);
        if (exit_status != 0) {
            return KIA_ERROR_SESSION;
        }
    } else if (WIFSIGNALED(status)) {
        logger_log(LOG_WARN, "Session terminated by signal %d", WTERMSIG(status));
        return KIA_ERROR_SESSION;
    }

    return KIA_SUCCESS;
}
