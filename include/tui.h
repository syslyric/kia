#ifndef KIA_TUI_H
#define KIA_TUI_H

#include <stddef.h>
#include "session.h"

/**
 * Initialize the TUI (ncurses)
 * Sets up ncurses with proper settings: cbreak, noecho, keypad
 * @return KIA_SUCCESS on success, KIA_ERROR_SYSTEM on error
 */
int tui_init(void);

/**
 * Cleanup the TUI and restore terminal state
 * Calls endwin() to properly close ncurses
 */
void tui_cleanup(void);

/**
 * Draw the login screen with welcome message, hostname, and version
 * @param hostname System hostname to display
 * @param version Kia version string to display
 */
void tui_draw_login_screen(const char *hostname, const char *version);

/**
 * Get user credentials from the TUI
 * Displays username and password fields with input handling
 * Password input is masked with asterisks
 * Handles Tab/Arrow keys for field navigation and Enter for submission
 * @param username Buffer to store username
 * @param user_len Maximum length of username buffer
 * @param password Buffer to store password
 * @param pass_len Maximum length of password buffer
 * @return KIA_SUCCESS on successful input, KIA_ERROR_SYSTEM on error
 */
int tui_get_credentials(char *username, size_t user_len,
                        char *password, size_t pass_len);

/**
 * Display session selection menu with arrow key navigation
 * @param sessions List of available sessions
 * @param default_idx Default session index to highlight
 * @return Selected session index, or -1 on error/cancel
 */
int tui_select_session(const session_list_t *sessions, int default_idx);

/**
 * Display an error message in distinct color
 * @param message Error message to display
 */
void tui_show_error(const char *message);

/**
 * Display a general feedback message
 * @param message Message to display
 */
void tui_show_message(const char *message);

#endif /* KIA_TUI_H */
