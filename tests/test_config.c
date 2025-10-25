#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

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

/* Helper function to create a temporary config file */
static char *create_temp_config(const char *content) {
    static char template[] = "/tmp/kia_test_XXXXXX";
    char *filename = strdup(template);
    int fd = mkstemp(filename);
    
    if (fd == -1) {
        free(filename);
        return NULL;
    }
    
    if (content != NULL) {
        write(fd, content, strlen(content));
    }
    
    close(fd);
    return filename;
}

/* Test: Valid configuration file parsing */
TEST(test_valid_config_parsing) {
    kia_config_t config;
    const char *content = 
        "autologin_enabled=true\n"
        "autologin_user=testuser\n"
        "default_session=gnome\n"
        "max_attempts=5\n"
        "enable_logs=false\n"
        "lockout_duration=120\n";
    
    char *filename = create_temp_config(content);
    ASSERT(filename != NULL);
    
    int result = config_load(filename, &config);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    ASSERT_TRUE(config.autologin_enabled);
    ASSERT_STR_EQ(config.autologin_user, "testuser");
    ASSERT_STR_EQ(config.default_session, "gnome");
    ASSERT_EQ(config.max_attempts, 5);
    ASSERT_FALSE(config.enable_logs);
    ASSERT_EQ(config.lockout_duration, 120);
    
    config_free(&config);
    unlink(filename);
    free(filename);
}

/* Test: Missing file with defaults */
TEST(test_missing_file_defaults) {
    kia_config_t config;
    
    int result = config_load("/nonexistent/path/config", &config);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Should have default values */
    ASSERT_FALSE(config.autologin_enabled);
    ASSERT_STR_EQ(config.autologin_user, "");
    ASSERT_STR_EQ(config.default_session, "xfce");
    ASSERT_EQ(config.max_attempts, 3);
    ASSERT_TRUE(config.enable_logs);
    ASSERT_EQ(config.lockout_duration, 60);
    
    config_free(&config);
}

/* Test: Invalid syntax handling */
TEST(test_invalid_syntax) {
    kia_config_t config;
    const char *content = 
        "autologin_enabled=true\n"
        "invalid_line_without_equals\n"
        "max_attempts=5\n";
    
    char *filename = create_temp_config(content);
    ASSERT(filename != NULL);
    
    int result = config_load(filename, &config);
    ASSERT_EQ(result, KIA_ERROR_CONFIG);
    
    /* Valid lines should still be parsed */
    ASSERT_TRUE(config.autologin_enabled);
    ASSERT_EQ(config.max_attempts, 5);
    
    config_free(&config);
    unlink(filename);
    free(filename);
}

/* Test: Boundary values for max_attempts (valid) */
TEST(test_max_attempts_boundary_valid) {
    kia_config_t config;
    
    /* Test minimum valid value (1) */
    const char *content1 = "max_attempts=1\n";
    char *filename1 = create_temp_config(content1);
    ASSERT(filename1 != NULL);
    
    int result1 = config_load(filename1, &config);
    ASSERT_EQ(result1, KIA_SUCCESS);
    ASSERT_EQ(config.max_attempts, 1);
    
    config_free(&config);
    unlink(filename1);
    free(filename1);
    
    /* Test maximum valid value (10) */
    const char *content2 = "max_attempts=10\n";
    char *filename2 = create_temp_config(content2);
    ASSERT(filename2 != NULL);
    
    int result2 = config_load(filename2, &config);
    ASSERT_EQ(result2, KIA_SUCCESS);
    ASSERT_EQ(config.max_attempts, 10);
    
    config_free(&config);
    unlink(filename2);
    free(filename2);
}

/* Test: Boundary values for max_attempts (invalid) */
TEST(test_max_attempts_boundary_invalid) {
    kia_config_t config;
    
    /* Test below minimum (0) */
    const char *content1 = "max_attempts=0\n";
    char *filename1 = create_temp_config(content1);
    ASSERT(filename1 != NULL);
    
    int result1 = config_load(filename1, &config);
    ASSERT_EQ(result1, KIA_ERROR_CONFIG);
    /* Should fall back to defaults */
    ASSERT_EQ(config.max_attempts, 3);
    
    config_free(&config);
    unlink(filename1);
    free(filename1);
    
    /* Test above maximum (11) */
    const char *content2 = "max_attempts=11\n";
    char *filename2 = create_temp_config(content2);
    ASSERT(filename2 != NULL);
    
    int result2 = config_load(filename2, &config);
    ASSERT_EQ(result2, KIA_ERROR_CONFIG);
    /* Should fall back to defaults */
    ASSERT_EQ(config.max_attempts, 3);
    
    config_free(&config);
    unlink(filename2);
    free(filename2);
}

/* Test: Comments and empty lines */
TEST(test_comments_and_empty_lines) {
    kia_config_t config;
    const char *content = 
        "# This is a comment\n"
        "\n"
        "autologin_enabled=true\n"
        "  # Another comment with leading space\n"
        "\n"
        "max_attempts=7\n"
        "\n";
    
    char *filename = create_temp_config(content);
    ASSERT(filename != NULL);
    
    int result = config_load(filename, &config);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    ASSERT_TRUE(config.autologin_enabled);
    ASSERT_EQ(config.max_attempts, 7);
    
    config_free(&config);
    unlink(filename);
    free(filename);
}

/* Test: Whitespace handling */
TEST(test_whitespace_handling) {
    kia_config_t config;
    const char *content = 
        "  autologin_enabled  =  true  \n"
        "autologin_user=  testuser  \n"
        "  max_attempts=5\n";
    
    char *filename = create_temp_config(content);
    ASSERT(filename != NULL);
    
    int result = config_load(filename, &config);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    ASSERT_TRUE(config.autologin_enabled);
    ASSERT_STR_EQ(config.autologin_user, "testuser");
    ASSERT_EQ(config.max_attempts, 5);
    
    config_free(&config);
    unlink(filename);
    free(filename);
}

/* Test: Boolean value parsing */
TEST(test_boolean_parsing) {
    kia_config_t config;
    const char *content = 
        "autologin_enabled=yes\n"
        "enable_logs=on\n";
    
    char *filename = create_temp_config(content);
    ASSERT(filename != NULL);
    
    int result = config_load(filename, &config);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    ASSERT_TRUE(config.autologin_enabled);
    ASSERT_TRUE(config.enable_logs);
    
    config_free(&config);
    unlink(filename);
    free(filename);
}

/* Test: Unknown keys are ignored */
TEST(test_unknown_keys_ignored) {
    kia_config_t config;
    const char *content = 
        "autologin_enabled=true\n"
        "unknown_key=some_value\n"
        "max_attempts=5\n";
    
    char *filename = create_temp_config(content);
    ASSERT(filename != NULL);
    
    int result = config_load(filename, &config);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    ASSERT_TRUE(config.autologin_enabled);
    ASSERT_EQ(config.max_attempts, 5);
    
    config_free(&config);
    unlink(filename);
    free(filename);
}

/* Test: config_validate function */
TEST(test_config_validate) {
    kia_config_t config;
    
    /* Valid configuration */
    config.max_attempts = 5;
    config.lockout_duration = 60;
    ASSERT_EQ(config_validate(&config), KIA_SUCCESS);
    
    /* Invalid max_attempts (too low) */
    config.max_attempts = 0;
    ASSERT_EQ(config_validate(&config), KIA_ERROR_CONFIG);
    
    /* Invalid max_attempts (too high) */
    config.max_attempts = 11;
    ASSERT_EQ(config_validate(&config), KIA_ERROR_CONFIG);
    
    /* Invalid lockout_duration (negative) */
    config.max_attempts = 5;
    config.lockout_duration = -1;
    ASSERT_EQ(config_validate(&config), KIA_ERROR_CONFIG);
    
    /* NULL pointer */
    ASSERT_EQ(config_validate(NULL), KIA_ERROR_CONFIG);
}

/* Main test runner */
int main(void) {
    printf("Running configuration parser tests...\n\n");
    
    test_valid_config_parsing_wrapper();
    test_missing_file_defaults_wrapper();
    test_invalid_syntax_wrapper();
    test_max_attempts_boundary_valid_wrapper();
    test_max_attempts_boundary_invalid_wrapper();
    test_comments_and_empty_lines_wrapper();
    test_whitespace_handling_wrapper();
    test_boolean_parsing_wrapper();
    test_unknown_keys_ignored_wrapper();
    test_config_validate_wrapper();
    
    printf("\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
