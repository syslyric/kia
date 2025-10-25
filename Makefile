# Kia Display Manager - Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpam -lncurses

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests
CONFIG_DIR = config

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET = kia

# Installation paths
PREFIX ?= /usr
BINDIR = $(PREFIX)/bin
SYSTEMDDIR = $(PREFIX)/lib/systemd/system
ETCDIR = /etc
LOGROTATE_DIR = $(ETCDIR)/logrotate.d

.PHONY: all clean install uninstall test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

install: $(TARGET)
	# Install binary with root-only execution permissions
	install -D -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	# Install systemd service
	install -D -m 644 $(CONFIG_DIR)/kia.service $(DESTDIR)$(SYSTEMDDIR)/kia.service
	# Install PAM configuration with restricted permissions
	install -D -m 644 $(CONFIG_DIR)/kia.pam $(DESTDIR)$(ETCDIR)/pam.d/kia
	# Install configuration file with proper permissions
	install -D -m 644 $(CONFIG_DIR)/kia.config $(DESTDIR)$(ETCDIR)/kia/config
	# Install logrotate configuration
	install -D -m 644 $(CONFIG_DIR)/kia.logrotate $(DESTDIR)$(LOGROTATE_DIR)/kia
	# Create log directory with proper permissions
	@mkdir -p $(DESTDIR)/var/log
	@touch $(DESTDIR)/var/log/kia.log || true
	@chmod 640 $(DESTDIR)/var/log/kia.log || true
	@echo "Installation complete. Enable with: systemctl enable kia"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(SYSTEMDDIR)/kia.service
	rm -f $(DESTDIR)$(ETCDIR)/pam.d/kia
	rm -f $(DESTDIR)$(ETCDIR)/kia/config
	rm -f $(DESTDIR)$(LOGROTATE_DIR)/kia
	@echo "Uninstallation complete"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

test:
	$(MAKE) -C $(TEST_DIR)
