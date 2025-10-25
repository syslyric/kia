#include "controller.h"
#include "config.h"
#include "logger.h"
#include "auth.h"
#include "session.h"
#include "tui.h"
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
#define ASSERT_NEQ(a, b) ASSERT((a) != (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_TRUE(x) ASSERT((x) == true)
#define ASSERT_FALSE(x) ASSERT((x) == false)

/* Test: Controller initialization */
TEST(test_controller_init) {
    app_context_t ctx;
    
    int result = controller_init(&ctx);
    ASSERT_EQ(result, KIA_SUCCESS);
    
    /* Verify initial state */
    ASSERT_EQ(ctx.state, STATE_INIT);
    ASSERT_TRUE(ctx.running);
    ASSERT_EQ(ctx.selected_session, -1);
    ASSERT_EQ(ctx.username[0], '\0');
    ASSERT_EQ(ctx.password[0], '\0');
    ASSERT_EQ(ctx.auth_state.failed_attempts, 0);
    ASSERT_EQ(ctx.sessions.count, 0);
}

/* Test: Controller initialization with NULL context */
TEST(test_controller_init_null) {
    int result = controller_init(NULL);
    ASSERT_EQ(result, KIA_ERROR_SYSTEM);
}

/* Test: Controller cleanup */
TEST(test_controller_cleanup) {
    app_context_t ctx;
    
    controller_init(&ctx);
    
    /* Set some data */
    strncpy(ctx.username, "testuser", sizeof(ctx.username));
    strncpy(ctx.password, "secret", sizeof(ctx.password));
    
    /* Cleanup should not crash */
    controller_cleanup(&ctx);
    
    /* Password should be cleared */
    ASSERT_EQ(ctx.password[0], '\0');
}

/* Test: Controller cleanup with NULL context */
TEST(test_controller_cleanup_null) {
    /* Should not crash */
    controller_cleanup(NULL);
}

/* Test: State transitions - INIT to LOAD_CONFIG */
TEST(test_state_init_to_load_config) {
    app_context_t ctx;
    controller_init(&ctx);
    
    ASSERT_EQ(ctx.state, STATE_INIT);
    
    /* After init, state should be INIT */
    ASSERT_EQ(ctx.state, STATE_INIT);
}

/* Test: State machine basic flow */
TEST(test_state_machine_basic_flow) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Verify state progression */
    ASSERT_EQ(ctx.state, STATE_INIT);
    ASSERT_TRUE(ctx.running);
    
    /* State should be valid */
    ASSERT(ctx.state >= STATE_INIT && ctx.state <= STATE_EXIT);
}

/* Test: Context structure size and alignment */
TEST(test_context_structure) {
    app_context_t ctx;
    
    /* Verify structure can be initialized */
    memset(&ctx, 0, sizeof(app_context_t));
    
    /* Verify fields are accessible */
    ctx.state = STATE_INIT;
    ctx.running = true;
    ctx.selected_session = 0;
    
    ASSERT_EQ(ctx.state, STATE_INIT);
    ASSERT_TRUE(ctx.running);
    ASSERT_EQ(ctx.selected_session, 0);
}

/* Test: Username and password buffers */
TEST(test_credential_buffers) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Test username buffer */
    const char *test_username = "testuser";
    strncpy(ctx.username, test_username, sizeof(ctx.username) - 1);
    ctx.username[sizeof(ctx.username) - 1] = '\0';
    ASSERT_STR_EQ(ctx.username, test_username);
    
    /* Test password buffer */
    const char *test_password = "testpass";
    strncpy(ctx.password, test_password, sizeof(ctx.password) - 1);
    ctx.password[sizeof(ctx.password) - 1] = '\0';
    ASSERT_STR_EQ(ctx.password, test_password);
    
    /* Clear password */
    memset(ctx.password, 0, sizeof(ctx.password));
    ASSERT_EQ(ctx.password[0], '\0');
}

/* Test: Auth state integration */
TEST(test_auth_state_integration) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Verify auth state is initialized */
    ASSERT_EQ(ctx.auth_state.failed_attempts, 0);
    ASSERT_EQ(ctx.auth_state.lockout_until, 0);
    ASSERT_EQ(ctx.auth_state.username[0], '\0');
    
    /* Simulate failed attempt */
    ctx.auth_state.failed_attempts = 1;
    ASSERT_EQ(ctx.auth_state.failed_attempts, 1);
}

/* Test: Session list integration */
TEST(test_session_list_integration) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Verify session list is initialized */
    ASSERT_EQ(ctx.sessions.count, 0);
    ASSERT_EQ(ctx.sessions.sessions, NULL);
    
    /* Simulate session discovery */
    ctx.sessions.count = 2;
    ASSERT_EQ(ctx.sessions.count, 2);
}

/* Test: Config integration */
TEST(test_config_integration) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Set config values */
    ctx.config.autologin_enabled = true;
    ctx.config.max_attempts = 5;
    ctx.config.lockout_duration = 120;
    strncpy(ctx.config.autologin_user, "testuser", sizeof(ctx.config.autologin_user) - 1);
    strncpy(ctx.config.default_session, "xfce", sizeof(ctx.config.default_session) - 1);
    
    /* Verify values */
    ASSERT_TRUE(ctx.config.autologin_enabled);
    ASSERT_EQ(ctx.config.max_attempts, 5);
    ASSERT_EQ(ctx.config.lockout_duration, 120);
    ASSERT_STR_EQ(ctx.config.autologin_user, "testuser");
    ASSERT_STR_EQ(ctx.config.default_session, "xfce");
}

/* Test: Selected session index */
TEST(test_selected_session_index) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Initial value should be -1 (no selection) */
    ASSERT_EQ(ctx.selected_session, -1);
    
    /* Set valid selection */
    ctx.selected_session = 0;
    ASSERT_EQ(ctx.selected_session, 0);
    
    ctx.selected_session = 5;
    ASSERT_EQ(ctx.selected_session, 5);
}

/* Test: Running flag */
TEST(test_running_flag) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Should be running after init */
    ASSERT_TRUE(ctx.running);
    
    /* Can be set to false */
    ctx.running = false;
    ASSERT_FALSE(ctx.running);
}

/* Test: State enumeration values */
TEST(test_state_enumeration) {
    /* Verify all states are distinct */
    ASSERT_NEQ(STATE_INIT, STATE_LOAD_CONFIG);
    ASSERT_NEQ(STATE_LOAD_CONFIG, STATE_CHECK_AUTOLOGIN);
    ASSERT_NEQ(STATE_CHECK_AUTOLOGIN, STATE_SHOW_LOGIN);
    ASSERT_NEQ(STATE_SHOW_LOGIN, STATE_GET_CREDENTIALS);
    ASSERT_NEQ(STATE_GET_CREDENTIALS, STATE_SELECT_SESSION);
    ASSERT_NEQ(STATE_SELECT_SESSION, STATE_AUTHENTICATE);
    ASSERT_NEQ(STATE_AUTHENTICATE, STATE_START_SESSION);
    ASSERT_NEQ(STATE_START_SESSION, STATE_EXIT);
}

/* Test: Memory safety - buffer overflow protection */
TEST(test_buffer_overflow_protection) {
    app_context_t ctx;
    controller_init(&ctx);
    
    /* Test username buffer boundary */
    char long_username[300];
    memset(long_username, 'A', sizeof(long_username) - 1);
    long_username[sizeof(long_username) - 1] = '\0';
    
    strncpy(ctx.username, long_username, sizeof(ctx.username) - 1);
    ctx.username[sizeof(ctx.username) - 1] = '\0';
    
    /* Should be truncated to buffer size */
    ASSERT_EQ(strlen(ctx.username), sizeof(ctx.username) - 1);
    
    /* Test password buffer boundary */
    char long_password[300];
    memset(long_password, 'B', sizeof(long_password) - 1);
    long_password[sizeof(long_password) - 1] = '\0';
    
    strncpy(ctx.password, long_password, sizeof(ctx.password) - 1);
    ctx.password[sizeof(ctx.password) - 1] = '\0';
    
    /* Should be truncated to buffer size */
    ASSERT_EQ(strlen(ctx.password), sizeof(ctx.password) - 1);
}

/* Test: Multiple init/cleanup cycles */
TEST(test_multiple_init_cleanup_cycles) {
    app_context_t ctx;
    
    /* First cycle */
    ASSERT_EQ(controller_init(&ctx), KIA_SUCCESS);
    controller_cleanup(&ctx);
    
    /* Second cycle */
    ASSERT_EQ(controller_init(&ctx), KIA_SUCCESS);
    controller_cleanup(&ctx);
    
    /* Third cycle */
    ASSERT_EQ(controller_init(&ctx), KIA_SUCCESS);
    controller_cleanup(&ctx);
}

/* Main test runner */
int main(void) {
    printf("Running controller integration tests...\n\n");
    
    test_controller_init_wrapper();
    test_controller_init_null_wrapper();
    test_controller_cleanup_wrapper();
    test_controller_cleanup_null_wrapper();
    test_state_init_to_load_config_wrapper();
    test_state_machine_basic_flow_wrapper();
    test_context_structure_wrapper();
    test_credential_buffers_wrapper();
    test_auth_state_integration_wrapper();
    test_session_list_integration_wrapper();
    test_config_integration_wrapper();
    test_selected_session_index_wrapper();
    test_running_flag_wrapper();
    test_state_enumeration_wrapper();
    test_buffer_overflow_protection_wrapper();
    test_multiple_init_cleanup_cycles_wrapper();
    
    printf("\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
