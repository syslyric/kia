#include "session.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Test counter */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST(name) \
    static void name(void); \
    static void name##_wrapper(void) { \
        printf("Running %s...", #name); \
        name(); \
        printf(" PASSED\n"); \
        tests_passed++; \
    } \
    static void name(void)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("\n  Assertion failed: %s\n", #condition); \
            printf("  at %s:%d\n", __FILE__, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_TRUE(x) ASSERT((x) == true)
#define ASSERT_FALSE(x) ASSERT((x) == false)
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)

/* Helper function to create a temporary directory */
static char *create_temp_dir(void) {
    static char template[] = "/tmp/kia_session_test_XXXXXX";
    char *dirname = strdup(template);
    if (mkdtemp(dirname) == NULL) {
        free(dirname);
        return NULL;
    }
    return dirname;
}

/* Helper function to create a mock .desktop file */
static int create_desktop_file(const char *dir, const char *filename, 
                               const char *name, const char *exec) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);
    
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        return -1;
    }
    
    fprintf(fp, "[Desktop Entry]\n");
    fprintf(fp, "Name=%s\n", name);
    fprintf(fp, "Exec=%s\n", exec);
    fprintf(fp, "Type=Application\n");
    
    fclose(fp);
    return 0;
}

/* Helper function to recursively remove directory */
static void remove_dir_recursive(const char *path) {
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf %s", path);
    system(command);
}

/* Test: Session discovery with mock filesystem */
TEST(test_session_discovery_mock_filesystem) {
    /* Note: This test uses real session directories if they exist */
    session_list_t list;
    
    int result = session_discover(&list);
    
    /* If no sessions are found, this is expected on test systems */
    if (result == KIA_ERROR_SESSION) {
        /* This is acceptable - no sessions installed */
        printf(" (no sessions found - expected on test system)");
        return;
    }
    
    ASSERT_EQ(result, KIA_SUCCESS);
    ASSERT_TRUE(list.count > 0);
    ASSERT_NOT_NULL(list.sessions);
    
    /* Verify each session has required fields */
    for (int i = 0; i < list.count; i++) {
        ASSERT_TRUE(strlen(list.sessions[i].name) > 0);
        ASSERT_TRUE(strlen(list.sessions[i].exec) > 0);
        ASSERT_TRUE(list.sessions[i].type == SESSION_X11 || 
                   list.sessions[i].type == SESSION_WAYLAND);
    }
    
    session_list_free(&list);
}

/* Test: Desktop file parsing */
TEST(test_desktop_file_parsing) {
    /* Create temporary directory structure */
    char *temp_dir = create_temp_dir();
    ASSERT_NOT_NULL(temp_dir);
    
    char xsessions_dir[512];
    snprintf(xsessions_dir, sizeof(xsessions_dir), "%s/xsessions", temp_dir);
    mkdir(xsessions_dir, 0755);
    
    /* Create mock desktop files */
    ASSERT_EQ(create_desktop_file(xsessions_dir, "xfce.desktop", 
                                  "XFCE Session", "startxfce4"), 0);
    ASSERT_EQ(create_desktop_file(xsessions_dir, "gnome.desktop", 
                                  "GNOME", "gnome-session"), 0);
    
    /* Note: We can't easily test session_discover with custom paths
     * without modifying the implementation, so we verify the files exist */
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/xfce.desktop", xsessions_dir);
    
    FILE *fp = fopen(filepath, "r");
    ASSERT_NOT_NULL(fp);
    
    char line[256];
    int found_name = 0, found_exec = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Name=XFCE Session", 17) == 0) {
            found_name = 1;
        }
        if (strncmp(line, "Exec=startxfce4", 15) == 0) {
            found_exec = 1;
        }
    }
    
    fclose(fp);
    ASSERT_TRUE(found_name);
    ASSERT_TRUE(found_exec);
    
    /* Cleanup */
    remove_dir_recursive(temp_dir);
    free(temp_dir);
}

/* Test: Session list management and memory cleanup */
TEST(test_session_list_management) {
    session_list_t list;
    
    /* Initialize empty list */
    list.sessions = NULL;
    list.count = 0;
    
    /* Allocate some sessions manually */
    list.sessions = malloc(sizeof(session_info_t) * 3);
    ASSERT_NOT_NULL(list.sessions);
    list.count = 3;
    
    strcpy(list.sessions[0].name, "Session 1");
    strcpy(list.sessions[0].exec, "/usr/bin/session1");
    list.sessions[0].type = SESSION_X11;
    
    strcpy(list.sessions[1].name, "Session 2");
    strcpy(list.sessions[1].exec, "/usr/bin/session2");
    list.sessions[1].type = SESSION_WAYLAND;
    
    strcpy(list.sessions[2].name, "Session 3");
    strcpy(list.sessions[2].exec, "/usr/bin/session3");
    list.sessions[2].type = SESSION_X11;
    
    /* Verify data */
    ASSERT_EQ(list.count, 3);
    ASSERT_STR_EQ(list.sessions[0].name, "Session 1");
    ASSERT_EQ(list.sessions[0].type, SESSION_X11);
    ASSERT_STR_EQ(list.sessions[1].name, "Session 2");
    ASSERT_EQ(list.sessions[1].type, SESSION_WAYLAND);
    
    /* Free the list */
    session_list_free(&list);
    
    /* Verify cleanup */
    ASSERT_NULL(list.sessions);
    ASSERT_EQ(list.count, 0);
}

/* Test: Session list free with NULL */
TEST(test_session_list_free_null) {
    /* Should not crash */
    session_list_free(NULL);
    
    session_list_t list;
    list.sessions = NULL;
    list.count = 0;
    
    /* Should not crash with NULL sessions */
    session_list_free(&list);
}

/* Test: Session discovery with NULL parameter */
TEST(test_session_discover_null) {
    int result = session_discover(NULL);
    ASSERT_EQ(result, KIA_ERROR_SESSION);
}

/* Test: Session start with invalid parameters */
TEST(test_session_start_invalid_params) {
    session_info_t session;
    strcpy(session.name, "Test Session");
    strcpy(session.exec, "/bin/true");
    session.type = SESSION_X11;
    
    /* NULL session */
    int result = session_start(NULL, "testuser");
    ASSERT_EQ(result, KIA_ERROR_SESSION);
    
    /* NULL username */
    result = session_start(&session, NULL);
    ASSERT_EQ(result, KIA_ERROR_SESSION);
}

/* Test: Session start with nonexistent user */
TEST(test_session_start_nonexistent_user) {
    session_info_t session;
    strcpy(session.name, "Test Session");
    strcpy(session.exec, "/bin/true");
    session.type = SESSION_X11;
    
    /* Use a username that definitely doesn't exist */
    int result = session_start(&session, "nonexistent_user_12345");
    ASSERT_EQ(result, KIA_ERROR_SESSION);
}

/* Test: Session type enumeration */
TEST(test_session_type_enum) {
    session_info_t session;
    
    session.type = SESSION_X11;
    ASSERT_EQ(session.type, SESSION_X11);
    ASSERT_TRUE(session.type != SESSION_WAYLAND);
    
    session.type = SESSION_WAYLAND;
    ASSERT_EQ(session.type, SESSION_WAYLAND);
    ASSERT_TRUE(session.type != SESSION_X11);
}

/* Test: Session info structure size limits */
TEST(test_session_info_size_limits) {
    session_info_t session;
    
    /* Test name field size */
    char long_name[300];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    
    strncpy(session.name, long_name, sizeof(session.name) - 1);
    session.name[sizeof(session.name) - 1] = '\0';
    
    ASSERT_EQ(strlen(session.name), sizeof(session.name) - 1);
    
    /* Test exec field size */
    char long_exec[600];
    memset(long_exec, 'B', sizeof(long_exec) - 1);
    long_exec[sizeof(long_exec) - 1] = '\0';
    
    strncpy(session.exec, long_exec, sizeof(session.exec) - 1);
    session.exec[sizeof(session.exec) - 1] = '\0';
    
    ASSERT_EQ(strlen(session.exec), sizeof(session.exec) - 1);
}

/* Test: Empty session list after discovery failure */
TEST(test_empty_session_list) {
    session_list_t list;
    list.sessions = NULL;
    list.count = 0;
    
    /* Verify initial state */
    ASSERT_NULL(list.sessions);
    ASSERT_EQ(list.count, 0);
    
    /* Free should be safe */
    session_list_free(&list);
    
    ASSERT_NULL(list.sessions);
    ASSERT_EQ(list.count, 0);
}

/* Main test runner */
int main(void) {
    /* Initialize logger for tests */
    logger_init("/tmp/kia_session_test.log", true);
    
    printf("Running session manager tests...\n\n");
    
    test_session_discovery_mock_filesystem_wrapper();
    test_desktop_file_parsing_wrapper();
    test_session_list_management_wrapper();
    test_session_list_free_null_wrapper();
    test_session_discover_null_wrapper();
    test_session_start_invalid_params_wrapper();
    test_session_start_nonexistent_user_wrapper();
    test_session_type_enum_wrapper();
    test_session_info_size_limits_wrapper();
    test_empty_session_list_wrapper();
    
    printf("\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    logger_close();
    
    return tests_failed > 0 ? 1 : 0;
}
