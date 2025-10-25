# Kia Display Manager - Test Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpam -lncurses

INC_DIR = ../include
SRC_DIR = ../src
BUILD_DIR = build

# Test sources will be added as tests are implemented
TEST_SOURCES = test_config.c test_logger.c test_auth.c test_session.c test_tui.c test_controller.c
TEST_TARGETS = $(TEST_SOURCES:%.c=$(BUILD_DIR)/%)

.PHONY: all clean run

all: $(TEST_TARGETS)

$(BUILD_DIR)/test_config: test_config.c $(SRC_DIR)/config.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@

$(BUILD_DIR)/test_logger: test_logger.c $(SRC_DIR)/logger.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@

$(BUILD_DIR)/test_auth: test_auth.c $(SRC_DIR)/auth.c $(SRC_DIR)/logger.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_session: test_session.c $(SRC_DIR)/session.c $(SRC_DIR)/logger.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@

$(BUILD_DIR)/test_tui: test_tui.c $(SRC_DIR)/tui.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_controller: test_controller.c $(SRC_DIR)/controller.c $(SRC_DIR)/config.c $(SRC_DIR)/logger.c $(SRC_DIR)/auth.c $(SRC_DIR)/session.c $(SRC_DIR)/tui.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $< -o $@ $(LDFLAGS)

run: all
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done

clean:
	rm -rf $(BUILD_DIR)
