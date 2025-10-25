# Kia Display Manager - Architecture

This document describes the architecture and design decisions for the Kia display manager.

For detailed design information, see the design document at `.kiro/specs/kia-display-manager/design.md`.

## Overview

Kia is a modular, lightweight display manager built with:
- C11 standard
- ncurses for TUI
- PAM for authentication
- systemd integration

## Component Overview

1. **Main Process** - Entry point and initialization
2. **Configuration Parser** - Reads and validates `/etc/kia/config`
3. **Logger** - Writes events to `/var/log/kia.log`
4. **Authentication Module** - PAM integration and lockout logic
5. **Session Manager** - Discovers and launches X11/Wayland sessions
6. **TUI Layer** - ncurses-based user interface
7. **Application Controller** - Coordinates all components

## Build System

The project uses Make with the following targets:
- `make` - Build the kia executable
- `make install` - Install to system directories
- `make clean` - Remove build artifacts
- `make test` - Run test suite
- `make uninstall` - Remove installed files

## Dependencies

- **libpam**: Authentication framework
- **ncurses**: Terminal UI library
- **systemd**: Service management

See README.md for distribution-specific package names.
