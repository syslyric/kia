#include "auth.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

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

/* Test: Authentication module initialization */
TEST(test_auth_init) {
    int result = auth_init();
    ASSERT_EQ(result, KIA_SUCCESS);
    
    auth_cleanup();
}

/* Test: Failed attempt tracking */
TEST(test_failed_attempt_tracking) {
    auth_state_t state = {0};
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    /* Initialize state */
    strncpy(state.username, "testuser", sizeof(state.username) - 1);
    state.failed_attempts = 0;
    state.lockout_until = 0;
    
    /* Simulate failed attempts by manually incrementing */
    state.failed_attempts = 1;
    ASSERT_EQ(state.failed_attempts, 1);
    ASSERT_FALSE(auth_is_locked_out(&state));
    
    state.failed_attempts = 2;
    ASSERT_EQ(state.failed_attempts, 2);
    ASSERT_FALSE(auth_is_locked_out(&state));
    
    /* Third failure should trigger lockout */
    state.failed_attempts = 3;
    state.lockout_until = time(NULL) + config.lockout_duration;
    ASSERT_EQ(state.failed_attempts, 3);
    ASSERT_TRUE(auth_is_locked_out(&state));
}

/* Test: Lockout mechanism with time */
TEST(test_lockout_mechanism) {
    auth_state_t state = {0};
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    strncpy(state.username, "testuser", sizeof(state.username) - 1);
    state.failed_attempts = 3;
    state.lockout_until = time(NULL) + config.lockout_duration;
    
    /* Should be locked out */
    ASSERT_TRUE(auth_is_locked_out(&state));
    
    /* Simulate time passing by setting lockout_until to the past */
    state.lockout_until = time(NULL) - 1;
    
    /* Should no longer be locked out */
    ASSERT_FALSE(auth_is_locked_out(&state));
    
    /* Failed attempts should be reset after lockout expires */
    ASSERT_EQ(state.failed_attempts, 0);
}

/* Test: Attempt reset logic */
TEST(test_attempt_reset) {
    auth_state_t state = {0};
    
    strncpy(state.username, "testuser", sizeof(state.username) - 1);
    state.failed_attempts = 5;
    state.lockout_until = time(NULL) + 60;
    
    /* Reset attempts */
    auth_reset_attempts(&state);
    
    ASSERT_EQ(state.failed_attempts, 0);
    ASSERT_EQ(state.lockout_until, 0);
    ASSERT_FALSE(auth_is_locked_out(&state));
}

/* Test: Edge case - empty username */
TEST(test_empty_username) {
    auth_state_t state = {0};
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    /* Initialize logger for auth module */
    logger_init("/tmp/kia_test_auth.log", true);
    auth_init();
    
    /* Empty username should fail */
    int result = auth_authenticate("", "password", &config, &state);
    ASSERT_EQ(result, KIA_ERROR_AUTH);
    
    auth_cleanup();
    logger_close();
    unlink("/tmp/kia_test_auth.log");
}

/* Test: Edge case - NULL password */
TEST(test_null_password) {
    auth_state_t state = {0};
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    /* Initialize logger for auth module */
    logger_init("/tmp/kia_test_auth.log", true);
    auth_init();
    
    /* NULL password should fail */
    int result = auth_authenticate("testuser", NULL, &config, &state);
    ASSERT_EQ(result, KIA_ERROR_AUTH);
    
    auth_cleanup();
    logger_close();
    unlink("/tmp/kia_test_auth.log");
}

/* Test: Edge case - NULL config */
TEST(test_null_config) {
    auth_state_t state = {0};
    
    /* Initialize logger for auth module */
    logger_init("/tmp/kia_test_auth.log", true);
    auth_init();
    
    /* NULL config should fail */
    int result = auth_authenticate("testuser", "password", NULL, &state);
    ASSERT_EQ(result, KIA_ERROR_AUTH);
    
    auth_cleanup();
    logger_close();
    unlink("/tmp/kia_test_auth.log");
}

/* Test: Edge case - NULL state */
TEST(test_null_state) {
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    /* Initialize logger for auth module */
    logger_init("/tmp/kia_test_auth.log", true);
    auth_init();
    
    /* NULL state should fail */
    int result = auth_authenticate("testuser", "password", &config, NULL);
    ASSERT_EQ(result, KIA_ERROR_AUTH);
    
    auth_cleanup();
    logger_close();
    unlink("/tmp/kia_test_auth.log");
}

/* Test: Username change resets attempts */
TEST(test_username_change_resets_attempts) {
    auth_state_t state = {0};
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    /* Initialize logger for auth module */
    logger_init("/tmp/kia_test_auth.log", true);
    auth_init();
    
    /* Set initial username and failed attempts */
    strncpy(state.username, "user1", sizeof(state.username) - 1);
    state.failed_attempts = 2;
    state.lockout_until = 0;
    
    /* Attempt with different username (will fail due to invalid credentials) */
    int result = auth_authenticate("user2", "wrongpass", &config, &state);
    ASSERT_EQ(result, KIA_ERROR_AUTH);
    
    /* Username should be updated and attempts should have been reset then incremented */
    ASSERT_STR_EQ(state.username, "user2");
    ASSERT_EQ(state.failed_attempts, 1);
    
    auth_cleanup();
    logger_close();
    unlink("/tmp/kia_test_auth.log");
}

/* Test: Lockout prevents authentication attempts */
TEST(test_lockout_prevents_auth) {
    auth_state_t state = {0};
    kia_config_t config = {
        .max_attempts = 3,
        .lockout_duration = 60
    };
    
    /* Initialize logger for auth module */
    logger_init("/tmp/kia_test_auth.log", true);
    auth_init();
    
    /* Set user as locked out */
    strncpy(state.username, "testuser", sizeof(state.username) - 1);
    state.failed_attempts = 3;
    state.lockout_until = time(NULL) + 60;
    
    /* Attempt should fail due to lockout */
    int result = auth_authenticate("testuser", "anypassword", &config, &state);
    ASSERT_EQ(result, KIA_ERROR_AUTH);
    ASSERT_TRUE(auth_is_locked_out(&state));
    
    auth_cleanup();
    logger_close();
    unlink("/tmp/kia_test_auth.log");
}

/* Test: auth_is_locked_out with NULL state */
TEST(test_is_locked_out_null_state) {
    bool result = auth_is_locked_out(NULL);
    ASSERT_FALSE(result);
}

/* Test: auth_reset_attempts with NULL state */
TEST(test_reset_attempts_null_state) {
    /* Should not crash */
    auth_reset_attempts(NULL);
}

/* Test: Multiple cleanup calls */
TEST(test_multiple_cleanup_calls) {
    auth_init();
    auth_cleanup();
    auth_cleanup();  /* Should not crash */
}

/* Main test runner */
int main(void) {
    printf("Running authentication module tests...\n\n");
    
    test_auth_init_wrapper();
    test_failed_attempt_tracking_wrapper();
    test_lockout_mechanism_wrapper();
    test_attempt_reset_wrapper();
    test_empty_username_wrapper();
    test_null_password_wrapper();
    test_null_config_wrapper();
    test_null_state_wrapper();
    test_username_change_resets_attempts_wrapper();
    test_lockout_prevents_auth_wrapper();
    test_is_locked_out_null_state_wrapper();
    test_reset_attempts_null_state_wrapper();
    test_multiple_cleanup_calls_wrapper();
    
    printf("\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
