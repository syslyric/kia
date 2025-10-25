#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>

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
#define ASSERT_TRUE(x) ASSERT((x) == true)
#define ASSERT_FALSE(x) ASSERT((x) == false)
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)

/* Helper function to create a temporary log file path */
static char *create_temp_log_path(void) {
    static char template[] = "/tmp/kia_log_test_XXXXXX";
    char *filename = strdup(template);
    int fd = mkstemp(filename);
    
    if (fd == -1) {
        free(filename);
        return NULL;
    }
    
    close(fd);
    unlink(filename);  /* Remove the file so logger can create it */
    return filename;
}

/* Helper function to read log file contents */
static char *read_log_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (content == NULL) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, size, file);
    content[size] = '\0';
    
    fclose(file);
    return content;
}

/* Helper function to count lines in a string */
static int count_lines(const char *str) {
    int count = 0;
    const char *p = str;
    
    while (*p) {
        if (*p == '\n') {
            count++;
        }
        p++;
    }
    
    return count;
}

/* Test: Log file creation */
TEST(test_log_file_creation) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    int result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Check that file was created */
    FILE *file = fopen(log_path, "r");
    ASSERT_NOT_NULL(file);
    fclose(file);
    
    logger_close();
    unlink(log_path);
    free(log_path);
}

/* Test: Log entry formatting with timestamps */
TEST(test_log_entry_formatting) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    int result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Write some log entries */
    logger_log(LOG_INFO, "Test message 1");
    logger_log(LOG_ERROR, "Test message 2");
    logger_log(LOG_WARN, "Test message 3");
    
    logger_close();
    
    /* Read log file and verify format */
    char *content = read_log_file(log_path);
    ASSERT_NOT_NULL(content);
    
    /* Check that we have 3 lines */
    ASSERT_EQ(count_lines(content), 3);
    
    /* Check for ISO 8601 timestamp format (YYYY-MM-DDTHH:MM:SSZ) */
    ASSERT(strstr(content, "T") != NULL);
    ASSERT(strstr(content, "Z") != NULL);
    
    /* Check for log levels */
    ASSERT(strstr(content, "[INFO]") != NULL);
    ASSERT(strstr(content, "[ERROR]") != NULL);
    ASSERT(strstr(content, "[WARN]") != NULL);
    
    /* Check for messages */
    ASSERT(strstr(content, "Test message 1") != NULL);
    ASSERT(strstr(content, "Test message 2") != NULL);
    ASSERT(strstr(content, "Test message 3") != NULL);
    
    free(content);
    unlink(log_path);
    free(log_path);
}

/* Test: Logging when disabled */
TEST(test_logging_disabled) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    /* Initialize with logging disabled */
    int result = logger_init(log_path, false);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Try to log messages */
    logger_log(LOG_INFO, "This should not be logged");
    logger_log(LOG_ERROR, "This should not be logged either");
    
    logger_close();
    
    /* Check that log file was not created */
    FILE *file = fopen(log_path, "r");
    ASSERT(file == NULL);
    
    free(log_path);
}

/* Test: File permission handling */
TEST(test_file_permissions) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    int result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    logger_log(LOG_INFO, "Test message");
    logger_close();
    
    /* Check file permissions (0640) */
    struct stat st;
    ASSERT_EQ(stat(log_path, &st), 0);
    
    /* Check that permissions are 0640 (owner read/write, group read) */
    mode_t perms = st.st_mode & 0777;
    ASSERT_EQ(perms, 0640);
    
    unlink(log_path);
    free(log_path);
}

/* Test: Multiple log levels */
TEST(test_multiple_log_levels) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    int result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Log at all levels */
    logger_log(LOG_DEBUG, "Debug message");
    logger_log(LOG_INFO, "Info message");
    logger_log(LOG_WARN, "Warning message");
    logger_log(LOG_ERROR, "Error message");
    
    logger_close();
    
    /* Read and verify */
    char *content = read_log_file(log_path);
    ASSERT_NOT_NULL(content);
    
    ASSERT(strstr(content, "[DEBUG]") != NULL);
    ASSERT(strstr(content, "[INFO]") != NULL);
    ASSERT(strstr(content, "[WARN]") != NULL);
    ASSERT(strstr(content, "[ERROR]") != NULL);
    
    free(content);
    unlink(log_path);
    free(log_path);
}

/* Test: Formatted log messages */
TEST(test_formatted_messages) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    int result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Log with format strings */
    logger_log(LOG_INFO, "User %s authenticated", "john");
    logger_log(LOG_ERROR, "Failed attempt %d of %d", 2, 3);
    logger_log(LOG_WARN, "Session type: %s", "X11");
    
    logger_close();
    
    /* Read and verify */
    char *content = read_log_file(log_path);
    ASSERT_NOT_NULL(content);
    
    ASSERT(strstr(content, "User john authenticated") != NULL);
    ASSERT(strstr(content, "Failed attempt 2 of 3") != NULL);
    ASSERT(strstr(content, "Session type: X11") != NULL);
    
    free(content);
    unlink(log_path);
    free(log_path);
}

/* Test: Append mode (multiple init/close cycles) */
TEST(test_append_mode) {
    char *log_path = create_temp_log_path();
    ASSERT_NOT_NULL(log_path);
    
    /* First session */
    int result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    logger_log(LOG_INFO, "First message");
    logger_close();
    
    /* Second session */
    result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    logger_log(LOG_INFO, "Second message");
    logger_close();
    
    /* Third session */
    result = logger_init(log_path, true);
    ASSERT_EQ(result, KIA_SUCCESS);
    logger_log(LOG_INFO, "Third message");
    logger_close();
    
    /* Read and verify all messages are present */
    char *content = read_log_file(log_path);
    ASSERT_NOT_NULL(content);
    
    ASSERT_EQ(count_lines(content), 3);
    ASSERT(strstr(content, "First message") != NULL);
    ASSERT(strstr(content, "Second message") != NULL);
    ASSERT(strstr(content, "Third message") != NULL);
    
    free(content);
    unlink(log_path);
    free(log_path);
}

/* Test: Invalid log path handling */
TEST(test_invalid_log_path) {
    /* Try to create log in non-existent directory */
    int result = logger_init("/nonexistent/directory/kia.log", true);
    
    /* Should return error but not crash */
    ASSERT_EQ(result, KIA_ERROR_SYSTEM);
    
    /* Logging should be disabled after failure */
    logger_log(LOG_INFO, "This should not crash");
    
    logger_close();
}

/* Test: NULL path handling */
TEST(test_null_path) {
    int result = logger_init(NULL, true);
    ASSERT_EQ(result, KIA_ERROR_SYSTEM);
    
    logger_close();
}

/* Main test runner */
int main(void) {
    printf("Running logger tests...\n\n");
    
    test_log_file_creation_wrapper();
    test_log_entry_formatting_wrapper();
    test_logging_disabled_wrapper();
    test_file_permissions_wrapper();
    test_multiple_log_levels_wrapper();
    test_formatted_messages_wrapper();
    test_append_mode_wrapper();
    test_invalid_log_path_wrapper();
    test_null_path_wrapper();
    
    printf("\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
