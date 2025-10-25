#include "tui.h"
#include "config.h"
#include <ncurses.h>
#include <string.h>
#include <unistd.h>

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
#define MAX_INPUT_LEN 255

/* Color pairs */
#define COLOR_NORMAL   1
#define COLOR_ERROR    2
#define COLOR_HIGHLIGHT 3
#define COLOR_MESSAGE  4

/* Field identifiers */
#define FIELD_USERNAME 0
#define FIELD_PASSWORD 1

int tui_init(void) {
    /* Initialize ncurses */
    if (initscr() == NULL) {
        return KIA_ERROR_SYSTEM;
    }

    /* Set up ncurses modes */
    cbreak();              /* Disable line buffering */
    noecho();              /* Don't echo input */
    keypad(stdscr, TRUE);  /* Enable function keys and arrow keys */
    curs_set(1);           /* Show cursor */

    /* Initialize colors if supported */
    if (has_colors()) {
        start_color();
        init_pair(COLOR_NORMAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_ERROR, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
        init_pair(COLOR_MESSAGE, COLOR_GREEN, COLOR_BLACK);
    }

    return KIA_SUCCESS;
}

void tui_cleanup(void) {
    endwin();
}

void tui_draw_login_screen(const char *hostname, const char *version) {
    int max_y, max_x;
    
    /* Validate stdscr is initialized */
    if (stdscr == NULL) {
        return;
    }
    
    getmaxyx(stdscr, max_y, max_x);
    
    /* Validate screen dimensions */
    if (max_y < 10 || max_x < 40) {
        /* Screen too small */
        clear();
        mvprintw(0, 0, "Terminal too small");
        refresh();
        return;
    }

    clear();

    /* Draw welcome message */
    int row = max_y / 4;
    const char *welcome = "Welcome to Kia";
    int welcome_len = strlen(welcome);
    int welcome_col = (max_x - welcome_len) / 2;
    if (welcome_col < 0) welcome_col = 0;
    mvprintw(row++, welcome_col, "%s", welcome);

    /* Draw hostname */
    row++;
    char hostname_str[512];
    const char *safe_hostname = (hostname && hostname[0] != '\0') ? hostname : "localhost";
    int len = snprintf(hostname_str, sizeof(hostname_str), "hostname: %s", safe_hostname);
    if (len > 0 && len < (int)sizeof(hostname_str)) {
        int col = (max_x - len) / 2;
        if (col < 0) col = 0;
        mvprintw(row++, col, "%s", hostname_str);
    }

    /* Draw version */
    char version_str[512];
    const char *safe_version = (version && version[0] != '\0') ? version : KIA_VERSION;
    len = snprintf(version_str, sizeof(version_str), "version: %s", safe_version);
    if (len > 0 && len < (int)sizeof(version_str)) {
        int col = (max_x - len) / 2;
        if (col < 0) col = 0;
        mvprintw(row++, col, "%s", version_str);
    }

    if (refresh() == ERR) {
        /* Refresh failed - not much we can do */
    }
}

int tui_get_credentials(char *username, size_t user_len,
                        char *password, size_t pass_len) {
    int max_y, max_x;
    int current_field = FIELD_USERNAME;
    int username_pos = 0;
    int password_pos = 0;
    
    /* Validate input parameters */
    if (username == NULL || password == NULL) {
        return KIA_ERROR_SYSTEM;
    }
    
    if (user_len == 0 || pass_len == 0) {
        return KIA_ERROR_SYSTEM;
    }
    
    /* Validate stdscr is initialized */
    if (stdscr == NULL) {
        return KIA_ERROR_SYSTEM;
    }
    
    getmaxyx(stdscr, max_y, max_x);
    
    /* Validate screen dimensions */
    if (max_y < 10 || max_x < 50) {
        return KIA_ERROR_SYSTEM;
    }

    /* Clear previous input */
    memset(username, 0, user_len);
    memset(password, 0, pass_len);

    /* Calculate field positions */
    int field_row = max_y / 2;
    int label_col = max_x / 2 - 20;
    int input_col = max_x / 2 - 5;
    
    /* Validate positions are within bounds */
    if (label_col < 0) label_col = 0;
    if (input_col < 0) input_col = 10;

    while (1) {
        /* Draw username field */
        mvprintw(field_row, label_col, "Username:");
        mvprintw(field_row, input_col, "[%-20s]", username);

        /* Draw password field */
        mvprintw(field_row + 2, label_col, "Password:");
        
        /* Mask password with asterisks - with bounds checking */
        char masked_password[MAX_INPUT_LEN + 1];
        if (password_pos >= 0 && password_pos <= MAX_INPUT_LEN) {
            memset(masked_password, '*', password_pos);
            masked_password[password_pos] = '\0';
            mvprintw(field_row + 2, input_col, "[%-20s]", masked_password);
        }

        /* Draw instruction */
        const char *instruction = "[Press Enter to login]";
        int instr_col = (max_x - strlen(instruction)) / 2;
        if (instr_col < 0) instr_col = 0;
        mvprintw(field_row + 4, instr_col, "%s", instruction);

        /* Position cursor */
        if (current_field == FIELD_USERNAME) {
            move(field_row, input_col + 1 + username_pos);
        } else {
            move(field_row + 2, input_col + 1 + password_pos);
        }

        if (refresh() == ERR) {
            return KIA_ERROR_SYSTEM;
        }

        /* Get input */
        int ch = getch();
        
        /* Check for input error */
        if (ch == ERR) {
            return KIA_ERROR_SYSTEM;
        }

        switch (ch) {
            case '\n':  /* Enter key */
            case KEY_ENTER:
                if (username_pos > 0) {
                    /* Ensure null termination */
                    username[username_pos] = '\0';
                    password[password_pos] = '\0';
                    return KIA_SUCCESS;
                }
                break;

            case '\t':  /* Tab key */
            case KEY_DOWN:
                current_field = (current_field == FIELD_USERNAME) ? FIELD_PASSWORD : FIELD_USERNAME;
                break;

            case KEY_UP:
                current_field = (current_field == FIELD_PASSWORD) ? FIELD_USERNAME : FIELD_PASSWORD;
                break;

            case KEY_BACKSPACE:
            case 127:  /* Backspace */
            case '\b':
                if (current_field == FIELD_USERNAME && username_pos > 0) {
                    username_pos--;
                    username[username_pos] = '\0';
                } else if (current_field == FIELD_PASSWORD && password_pos > 0) {
                    password_pos--;
                    /* Securely clear the removed character */
                    secure_memzero(&password[password_pos], 1);
                }
                break;

            case 27:  /* Escape key */
                if (current_field == FIELD_USERNAME) {
                    memset(username, 0, user_len);
                    username_pos = 0;
                } else {
                    secure_memzero(password, pass_len);
                    password_pos = 0;
                }
                break;

            default:
                /* Regular character input */
                if (ch >= 32 && ch <= 126) {  /* Printable ASCII */
                    if (current_field == FIELD_USERNAME && username_pos < (int)user_len - 1 && username_pos < 20) {
                        username[username_pos++] = (char)ch;
                        username[username_pos] = '\0';
                    } else if (current_field == FIELD_PASSWORD && password_pos < (int)pass_len - 1 && password_pos < 20) {
                        password[password_pos++] = (char)ch;
                        password[password_pos] = '\0';
                    }
                }
                break;
        }
    }

    return KIA_SUCCESS;
}

int tui_select_session(const session_list_t *sessions, int default_idx) {
    int max_y, max_x;
    int selected;
    int start_row;
    
    /* Validate input parameters */
    if (sessions == NULL || sessions->count == 0) {
        return -1;
    }
    
    if (sessions->sessions == NULL) {
        return -1;
    }
    
    /* Validate stdscr is initialized */
    if (stdscr == NULL) {
        return -1;
    }

    getmaxyx(stdscr, max_y, max_x);
    
    /* Validate screen dimensions */
    if (max_y < 10 || max_x < 60) {
        return -1;
    }

    /* Validate and set default selection */
    if (default_idx >= 0 && default_idx < sessions->count) {
        selected = default_idx;
    } else {
        selected = 0;
    }
    
    start_row = max_y / 3;
    
    /* Ensure we have room for all sessions */
    if (start_row + sessions->count + 4 > max_y) {
        start_row = 2;
    }

    while (1) {
        if (clear() == ERR) {
            return -1;
        }

        /* Draw title */
        const char *title = "Select Session";
        int title_col = (max_x - strlen(title)) / 2;
        if (title_col < 0) title_col = 0;
        mvprintw(start_row - 2, title_col, "%s", title);

        /* Draw session list */
        for (int i = 0; i < sessions->count; i++) {
            int row = start_row + i;
            
            /* Validate row is within screen bounds */
            if (row >= max_y - 3) {
                break;
            }
            
            /* Validate session name is not empty */
            if (sessions->sessions[i].name[0] == '\0') {
                continue;
            }
            
            int col = max_x / 2 - 20;
            if (col < 0) col = 0;
            
            if (i == selected) {
                if (has_colors()) {
                    attron(COLOR_PAIR(COLOR_HIGHLIGHT));
                }
                mvprintw(row, col, "> %-38s", sessions->sessions[i].name);
                if (has_colors()) {
                    attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
                }
            } else {
                mvprintw(row, col, "  %-38s", sessions->sessions[i].name);
            }

            /* Show session type */
            const char *type_str = (sessions->sessions[i].type == SESSION_X11) ? "[X11]" : "[Wayland]";
            int type_col = max_x / 2 + 20;
            if (type_col < max_x - 10) {
                mvprintw(row, type_col, "%s", type_str);
            }
        }

        /* Draw instructions */
        const char *instruction = "Use arrow keys to navigate, Enter to select";
        int instr_col = (max_x - strlen(instruction)) / 2;
        if (instr_col < 0) instr_col = 0;
        int instr_row = start_row + sessions->count + 2;
        if (instr_row < max_y) {
            mvprintw(instr_row, instr_col, "%s", instruction);
        }

        if (refresh() == ERR) {
            return -1;
        }

        /* Get input */
        int ch = getch();
        
        /* Check for input error */
        if (ch == ERR) {
            return -1;
        }

        switch (ch) {
            case '\n':  /* Enter key */
            case KEY_ENTER:
                /* Validate selection before returning */
                if (selected >= 0 && selected < sessions->count) {
                    return selected;
                }
                return -1;

            case KEY_UP:
                if (selected > 0) {
                    selected--;
                }
                break;

            case KEY_DOWN:
                if (selected < sessions->count - 1) {
                    selected++;
                }
                break;

            case 27:  /* Escape key */
                return -1;

            default:
                break;
        }
    }

    return selected;
}

void tui_show_error(const char *message) {
    int max_y, max_x;
    int row, col;
    size_t msg_len;
    
    /* Validate input */
    if (message == NULL || message[0] == '\0') {
        return;
    }
    
    /* Validate stdscr is initialized */
    if (stdscr == NULL) {
        return;
    }
    
    getmaxyx(stdscr, max_y, max_x);
    
    /* Validate screen dimensions */
    if (max_y < 3 || max_x < 10) {
        return;
    }

    row = max_y - 3;
    if (row < 0) row = 0;

    /* Clear the error line */
    move(row, 0);
    clrtoeol();

    /* Calculate message position with bounds checking */
    msg_len = strlen(message);
    if (msg_len > (size_t)max_x) {
        /* Message too long - truncate */
        char truncated[512];
        size_t max_msg_len = (max_x > 3) ? max_x - 3 : 0;
        if (max_msg_len > sizeof(truncated) - 1) {
            max_msg_len = sizeof(truncated) - 1;
        }
        strncpy(truncated, message, max_msg_len);
        truncated[max_msg_len] = '\0';
        col = 0;
        
        /* Display truncated error message in red */
        if (has_colors()) {
            attron(COLOR_PAIR(COLOR_ERROR));
        }
        mvprintw(row, col, "%s...", truncated);
        if (has_colors()) {
            attroff(COLOR_PAIR(COLOR_ERROR));
        }
    } else {
        col = (max_x - msg_len) / 2;
        if (col < 0) col = 0;
        
        /* Display error message in red */
        if (has_colors()) {
            attron(COLOR_PAIR(COLOR_ERROR));
        }
        mvprintw(row, col, "%s", message);
        if (has_colors()) {
            attroff(COLOR_PAIR(COLOR_ERROR));
        }
    }

    refresh();

    /* Wait for user to see the error */
    napms(2000);  /* 2 seconds */
}

void tui_show_message(const char *message) {
    int max_y, max_x;
    int row, col;
    size_t msg_len;
    
    /* Validate input */
    if (message == NULL || message[0] == '\0') {
        return;
    }
    
    /* Validate stdscr is initialized */
    if (stdscr == NULL) {
        return;
    }
    
    getmaxyx(stdscr, max_y, max_x);
    
    /* Validate screen dimensions */
    if (max_y < 3 || max_x < 10) {
        return;
    }

    row = max_y - 3;
    if (row < 0) row = 0;

    /* Clear the message line */
    move(row, 0);
    clrtoeol();

    /* Calculate message position with bounds checking */
    msg_len = strlen(message);
    if (msg_len > (size_t)max_x) {
        /* Message too long - truncate */
        char truncated[512];
        size_t max_msg_len = (max_x > 3) ? max_x - 3 : 0;
        if (max_msg_len > sizeof(truncated) - 1) {
            max_msg_len = sizeof(truncated) - 1;
        }
        strncpy(truncated, message, max_msg_len);
        truncated[max_msg_len] = '\0';
        col = 0;
        
        /* Display truncated message in green */
        if (has_colors()) {
            attron(COLOR_PAIR(COLOR_MESSAGE));
        }
        mvprintw(row, col, "%s...", truncated);
        if (has_colors()) {
            attroff(COLOR_PAIR(COLOR_MESSAGE));
        }
    } else {
        col = (max_x - msg_len) / 2;
        if (col < 0) col = 0;
        
        /* Display message in green */
        if (has_colors()) {
            attron(COLOR_PAIR(COLOR_MESSAGE));
        }
        mvprintw(row, col, "%s", message);
        if (has_colors()) {
            attroff(COLOR_PAIR(COLOR_MESSAGE));
        }
    }

    refresh();

    /* Wait for user to see the message */
    napms(1500);  /* 1.5 seconds */
}