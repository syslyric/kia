/**
 * Kia Display Manager - Main Entry Point
 * 
 * Lightweight TUI-based display manager for Linux
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include "config.h"
#include "logger.h"
#include "controller.h"

#define KIA_VERSION "1.0.0"
#define DEFAULT_CONFIG_PATH "/etc/kia/config"
#define DEFAULT_LOG_PATH "/var/log/kia.log"

/* Global context for signal handlers */
static app_context_t *g_app_context = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

/**
 * Signal handler for SIGTERM and SIGINT
 * Sets shutdown flag for graceful exit
 */
static void signal_handler(int signum) {
    (void)signum;  /* Unused parameter */
    g_shutdown_requested = 1;
}

/**
 * Cleanup and exit the application
 * Ensures all resources are properly freed
 */
static void cleanup_and_exit(int status) {
    if (g_app_context != NULL) {
        controller_cleanup(g_app_context);
        g_app_context = NULL;
    }
    
    logger_log(LOG_INFO, "Kia display manager shutting down");
    logger_close();
    
    exit(status);
}

/**
 * Setup signal handlers for graceful shutdown
 */
static int setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "Error: Failed to setup SIGTERM handler\n");
        return KIA_ERROR_SYSTEM;
    }
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Error: Failed to setup SIGINT handler\n");
        return KIA_ERROR_SYSTEM;
    }
    
    return KIA_SUCCESS;
}

/**
 * Check if running with root privileges
 */
static int check_root_privileges(void) {
    if (geteuid() != 0) {
        fprintf(stderr, "Error: Kia must be run as root\n");
        fprintf(stderr, "Please run with sudo or as root user\n");
        return KIA_ERROR_SYSTEM;
    }
    return KIA_SUCCESS;
}

/**
 * Display version information
 */
static void print_version(void) {
    printf("Kia Display Manager v%s\n", KIA_VERSION);
    printf("Lightweight TUI-based display manager for Linux\n");
}

/**
 * Display help information
 */
static void print_help(void) {
    printf("Usage: kia [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  --version    Display version information\n");
    printf("  --help       Display this help message\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  Config file: %s\n", DEFAULT_CONFIG_PATH);
    printf("  Log file:    %s\n", DEFAULT_LOG_PATH);
    printf("\n");
    printf("Kia must be run as root to manage user sessions.\n");
}

/**
 * Parse command-line arguments
 * Returns 0 to continue, 1 to exit successfully
 */
static int parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 1;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'kia --help' for more information\n");
            return -1;
        }
    }
    return 0;
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    int result;
    app_context_t app_context;
    
    /* Parse command-line arguments */
    result = parse_arguments(argc, argv);
    if (result != 0) {
        return (result > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    /* Check for root privileges */
    if (check_root_privileges() != KIA_SUCCESS) {
        return EXIT_FAILURE;
    }
    
    /* Setup signal handlers */
    if (setup_signal_handlers() != KIA_SUCCESS) {
        return EXIT_FAILURE;
    }
    
    /* Initialize logger (will be reconfigured after loading config) */
    if (logger_init(DEFAULT_LOG_PATH, true) != KIA_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize logger\n");
        /* Continue without logging */
    }
    
    logger_log(LOG_INFO, "Kia display manager v%s starting", KIA_VERSION);
    
    /* Initialize controller */
    g_app_context = &app_context;
    result = controller_init(&app_context);
    if (result != KIA_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize controller: %d", result);
        cleanup_and_exit(EXIT_FAILURE);
    }
    
    /* Load configuration */
    result = config_load(DEFAULT_CONFIG_PATH, &app_context.config);
    if (result != KIA_SUCCESS) {
        logger_log(LOG_WARN, "Failed to load config from %s, using defaults", 
                   DEFAULT_CONFIG_PATH);
    }
    
    /* Reconfigure logger based on config */
    logger_close();
    if (logger_init(DEFAULT_LOG_PATH, app_context.config.enable_logs) != KIA_SUCCESS) {
        fprintf(stderr, "Warning: Failed to reinitialize logger\n");
    }
    
    logger_log(LOG_INFO, "Configuration loaded, logging %s", 
               app_context.config.enable_logs ? "enabled" : "disabled");
    
    /* Run main controller loop */
    logger_log(LOG_INFO, "Starting main controller loop");
    result = controller_run(&app_context);
    
    if (result != KIA_SUCCESS) {
        logger_log(LOG_ERROR, "Controller exited with error: %d", result);
    }
    
    /* Check if shutdown was requested by signal */
    if (g_shutdown_requested) {
        logger_log(LOG_INFO, "Shutdown requested by signal");
    }
    
    /* Cleanup and exit */
    cleanup_and_exit((result == KIA_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE);
    
    return EXIT_SUCCESS;  /* Never reached */
}
