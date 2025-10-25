# Kia Display Manager

A lightweight, security-focused TUI-based display manager for Linux systems, designed as a minimal alternative to traditional display managers like GDM or LightDM. Written in C with ncurses, Kia provides a fast, efficient login experience with full support for both X11 and Wayland sessions.

## Features

- **Lightweight**: Minimal resource usage with ncurses-based TUI - no heavy GUI dependencies
- **Secure**: PAM-based authentication with configurable lockout protection after failed attempts
- **Flexible**: Automatic discovery and support for both X11 and Wayland sessions
- **Integrated**: Native systemd service with proper TTY management on tty7
- **Configurable**: Simple INI-style configuration file for customization
- **Auditable**: Comprehensive logging of authentication events with ISO 8601 timestamps
- **Autologin**: Optional autologin support for kiosk systems
- **Cross-Distribution**: Works on Debian, Arch, Alpine, and other major Linux distributions

## Dependencies

### Debian/Ubuntu
```bash
sudo apt-get install build-essential libpam0g-dev libncurses-dev systemd
```

### Arch Linux
```bash
sudo pacman -S base-devel pam ncurses systemd
```

### Alpine Linux
```bash
sudo apk add build-base linux-pam-dev ncurses-dev systemd-dev
```

## Building

```bash
make
```

## Installation

```bash
sudo make install
```

This will install:
- `/usr/bin/kia` - Main executable
- `/etc/kia/config` - Configuration file
- `/etc/pam.d/kia` - PAM configuration
- `/usr/lib/systemd/system/kia.service` - systemd unit
- `/etc/logrotate.d/kia` - Log rotation config

## Configuration

Kia is configured through `/etc/kia/config` using a simple key=value format. Edit this file to customize behavior:

```ini
# Enable autologin for a specific user (useful for kiosk systems)
# When enabled, Kia will automatically log in the specified user without prompting
autologin_enabled=false
autologin_user=

# Default session to launch
# This should match a session name from /usr/share/xsessions/ or /usr/share/wayland-sessions/
# Examples: xfce, gnome, kde, sway
default_session=xfce

# Maximum authentication attempts before lockout
# Valid range: 1-10
# After this many failed attempts, the user will be locked out temporarily
max_attempts=3

# Enable logging to /var/log/kia.log
# Logs include authentication events, errors, and session launches
enable_logs=true

# Lockout duration in seconds after max_attempts is reached
# Default: 60 seconds (1 minute)
lockout_duration=60
```

### Configuration Options Reference

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `autologin_enabled` | boolean | `false` | Enable automatic login without password prompt |
| `autologin_user` | string | (empty) | Username to automatically log in (requires autologin_enabled=true) |
| `default_session` | string | `xfce` | Default session to launch (must exist in session directories) |
| `max_attempts` | integer | `3` | Maximum failed login attempts before lockout (1-10) |
| `enable_logs` | boolean | `true` | Enable logging to /var/log/kia.log |
| `lockout_duration` | integer | `60` | Seconds to lock out user after max_attempts failures |

**Note**: If the configuration file is missing or contains invalid values, Kia will use the default values shown above and log a warning.

## Enabling Kia

Before enabling Kia, disable your current display manager:

```bash
# Disable GDM
sudo systemctl disable gdm

# Or disable LightDM
sudo systemctl disable lightdm

# Or disable SDDM
sudo systemctl disable sddm
```

Then enable and start Kia:

```bash
sudo systemctl enable kia
sudo systemctl start kia
```

Or simply reboot to start Kia automatically.

## Troubleshooting

### Kia doesn't start after enabling

**Symptoms**: Black screen or TTY7 doesn't show Kia interface

**Solutions**:
1. Check systemd service status:
   ```bash
   sudo systemctl status kia
   ```

2. View detailed logs:
   ```bash
   sudo journalctl -u kia -n 50
   ```

3. Verify Kia binary exists and is executable:
   ```bash
   ls -la /usr/bin/kia
   ```

4. Ensure no other display manager is running:
   ```bash
   systemctl list-units --type=service | grep -E 'gdm|lightdm|sddm'
   ```

5. Check PAM configuration exists:
   ```bash
   ls -la /etc/pam.d/kia
   ```

### No sessions available / "No sessions found" error

**Symptoms**: Kia starts but shows no sessions to select

**Solutions**:
1. Verify session files exist:
   ```bash
   ls /usr/share/xsessions/
   ls /usr/share/wayland-sessions/
   ```

2. Install a desktop environment if none are present:
   ```bash
   # Debian/Ubuntu - XFCE example
   sudo apt-get install xfce4
   
   # Arch - XFCE example
   sudo pacman -S xfce4
   ```

3. Check session file format (should be .desktop files):
   ```bash
   cat /usr/share/xsessions/xfce.desktop
   ```

4. Verify session files have proper permissions (should be readable):
   ```bash
   sudo chmod 644 /usr/share/xsessions/*.desktop
   ```

### Authentication fails for valid credentials

**Symptoms**: Correct password is rejected

**Solutions**:
1. Check PAM configuration is correct:
   ```bash
   cat /etc/pam.d/kia
   ```
   Should contain at minimum:
   ```
   auth       required     pam_unix.so
   account    required     pam_unix.so
   session    required     pam_unix.so
   ```

2. Verify user exists and can authenticate:
   ```bash
   id username
   su - username  # Test if password works
   ```

3. Check if user is locked out:
   ```bash
   sudo tail -n 20 /var/log/kia.log
   ```
   Look for lockout messages

4. Check PAM system logs:
   ```bash
   sudo journalctl -t kia | tail -n 20
   ```

5. Verify shadow file permissions:
   ```bash
   ls -la /etc/shadow
   # Should be: -rw-r----- 1 root shadow
   ```

### User is locked out after failed attempts

**Symptoms**: "Account locked" or similar message after multiple failed attempts

**Solutions**:
1. Wait for lockout duration to expire (default: 60 seconds)

2. Check lockout status in logs:
   ```bash
   sudo tail /var/log/kia.log
   ```

3. Adjust lockout settings in `/etc/kia/config`:
   ```ini
   max_attempts=5
   lockout_duration=30
   ```

4. Restart Kia to apply new settings:
   ```bash
   sudo systemctl restart kia
   ```

### Session starts but immediately exits

**Symptoms**: Login succeeds but returns to login screen

**Solutions**:
1. Check session logs:
   ```bash
   cat ~/.xsession-errors  # For X11 sessions
   journalctl --user -n 50  # For user session logs
   ```

2. Verify session executable exists:
   ```bash
   # Check the Exec= line in the .desktop file
   cat /usr/share/xsessions/your-session.desktop
   which startxfce4  # Example for XFCE
   ```

3. Test session manually:
   ```bash
   startx  # For X11
   # or
   sway    # For Wayland example
   ```

4. Check environment variables are set correctly in Kia logs:
   ```bash
   sudo grep "Starting.*session" /var/log/kia.log
   ```

### Log file permission errors

**Symptoms**: Errors about unable to write to /var/log/kia.log

**Solutions**:
1. Create log file with proper permissions:
   ```bash
   sudo touch /var/log/kia.log
   sudo chmod 640 /var/log/kia.log
   sudo chown root:root /var/log/kia.log
   ```

2. Verify log directory exists and is writable:
   ```bash
   ls -ld /var/log
   ```

### TTY7 shows garbled text or incorrect display

**Symptoms**: Display corruption or strange characters

**Solutions**:
1. Reset TTY:
   ```bash
   # Switch to another TTY (Ctrl+Alt+F2)
   sudo systemctl restart kia
   ```

2. Check terminal type:
   ```bash
   echo $TERM
   # Should be something like 'linux' or 'xterm'
   ```

3. Verify ncurses is properly installed:
   ```bash
   # Debian/Ubuntu
   dpkg -l | grep libncurses
   
   # Arch
   pacman -Q ncurses
   ```

### Kia conflicts with existing display manager

**Symptoms**: Both Kia and another DM try to start

**Solutions**:
1. Ensure other display managers are disabled:
   ```bash
   sudo systemctl disable gdm
   sudo systemctl disable lightdm
   sudo systemctl disable sddm
   sudo systemctl mask gdm  # Prevent accidental start
   ```

2. Check for conflicts:
   ```bash
   systemctl list-units --type=service --state=active | grep -E 'display|dm'
   ```

3. Verify Kia service has proper conflicts defined:
   ```bash
   systemctl cat kia | grep Conflicts
   ```

## Security Recommendations

### File Permissions

Kia requires specific file permissions for secure operation. The `make install` command sets these automatically, but verify them if you experience issues:

```bash
# Kia binary - executable by all, writable only by root
sudo chmod 755 /usr/bin/kia
sudo chown root:root /usr/bin/kia

# Configuration file - readable by all, writable only by root
sudo chmod 644 /etc/kia/config
sudo chown root:root /etc/kia/config

# PAM configuration - readable by all, writable only by root
sudo chmod 644 /etc/pam.d/kia
sudo chown root:root /etc/pam.d/kia

# Log file - readable by root only, writable by root
sudo chmod 640 /var/log/kia.log
sudo chown root:root /var/log/kia.log

# Configuration directory
sudo chmod 755 /etc/kia
sudo chown root:root /etc/kia
```

### PAM Configuration

The default PAM configuration (`/etc/pam.d/kia`) provides basic authentication. Review and customize it for your security requirements:

**Default Configuration**:
```
auth       required     pam_unix.so
auth       required     pam_env.so
account    required     pam_unix.so
password   required     pam_unix.so
session    required     pam_unix.so
session    required     pam_limits.so
```

**Enhanced Security Options**:

1. **Add fail delay** (slow down brute force attacks):
   ```
   auth       optional     pam_faildelay.so delay=4000000
   ```

2. **Deny root login** (prevent root from logging in via Kia):
   ```
   auth       required     pam_succeed_if.so user != root quiet
   ```

3. **Require group membership** (only allow specific groups):
   ```
   auth       required     pam_succeed_if.so user ingroup loginusers
   ```

4. **Add system-wide fail locking** (persistent lockout across reboots):
   ```
   auth       required     pam_faillock.so preauth
   auth       required     pam_faillock.so authfail
   account    required     pam_faillock.so
   ```

5. **Time-based access control** (restrict login hours):
   ```
   account    required     pam_time.so
   ```
   Then configure `/etc/security/time.conf`

**Note**: Test PAM changes carefully. Incorrect configuration can lock you out of the system. Always keep a root terminal open when testing.

### Lockout Settings

Configure appropriate lockout settings in `/etc/kia/config` based on your security needs:

- **High Security Environment**: `max_attempts=3`, `lockout_duration=300` (5 minutes)
- **Standard Environment**: `max_attempts=3`, `lockout_duration=60` (1 minute)
- **Low Security/Development**: `max_attempts=5`, `lockout_duration=30` (30 seconds)

### Log Monitoring

Regularly monitor `/var/log/kia.log` for suspicious activity:

```bash
# View recent authentication attempts
sudo tail -f /var/log/kia.log

# Search for failed authentication attempts
sudo grep "Authentication failed" /var/log/kia.log

# Count failed attempts by user
sudo grep "Authentication failed" /var/log/kia.log | awk '{print $6}' | sort | uniq -c

# Check for lockout events
sudo grep "locked out" /var/log/kia.log
```

Consider setting up automated monitoring with tools like `logwatch` or `fail2ban`.

### Additional Security Measures

1. **Disable Autologin in Production**: Only use autologin for kiosk systems or development
   ```ini
   autologin_enabled=false
   ```

2. **Keep System Updated**: Regularly update Kia and its dependencies
   ```bash
   # Rebuild and reinstall after updates
   git pull
   make clean
   make
   sudo make install
   sudo systemctl restart kia
   ```

3. **Audit User Accounts**: Regularly review which users can log in
   ```bash
   # List users with login shells
   grep -v '/nologin\|/false' /etc/passwd
   ```

4. **Secure TTY Access**: Ensure physical access to the system is controlled, as TTY switching (Ctrl+Alt+F1-F7) can bypass the login screen

5. **Monitor systemd Journal**: Check for PAM and authentication errors
   ```bash
   sudo journalctl -u kia --since today
   ```

6. **Use Strong Passwords**: Enforce password policies via PAM modules like `pam_pwquality`

7. **Consider Two-Factor Authentication**: Add PAM modules like `pam_google_authenticator` for 2FA

8. **Restrict Session Types**: Remove unnecessary session files from `/usr/share/xsessions/` and `/usr/share/wayland-sessions/`

9. **SELinux/AppArmor**: Consider creating security profiles for Kia if using mandatory access control systems

10. **Regular Backups**: Backup configuration files before making changes
    ```bash
    sudo cp /etc/kia/config /etc/kia/config.backup
    sudo cp /etc/pam.d/kia /etc/pam.d/kia.backup
    ```

## Migration from Other Display Managers

This guide helps you safely migrate from GDM, LightDM, SDDM, or other display managers to Kia.

### Pre-Migration Checklist

Before migrating, ensure you have:
- [ ] Root access to the system
- [ ] A backup of important data
- [ ] Access to a TTY (Ctrl+Alt+F2) in case of issues
- [ ] Knowledge of your current display manager
- [ ] At least one desktop environment installed

### Step-by-Step Migration Guide

#### Step 1: Identify Current Display Manager

```bash
# Check which display manager is currently running
systemctl status display-manager.service

# Or check for specific display managers
systemctl is-active gdm
systemctl is-active lightdm
systemctl is-active sddm
systemctl is-active lxdm
```

#### Step 2: Backup Current Configuration

```bash
# Create backup directory
sudo mkdir -p /root/dm-backup

# Backup GDM configuration (if using GDM)
sudo cp -r /etc/gdm* /root/dm-backup/ 2>/dev/null

# Backup LightDM configuration (if using LightDM)
sudo cp -r /etc/lightdm* /root/dm-backup/ 2>/dev/null

# Backup SDDM configuration (if using SDDM)
sudo cp -r /etc/sddm* /root/dm-backup/ 2>/dev/null

# Note current display manager
systemctl status display-manager.service > /root/dm-backup/previous-dm.txt
```

#### Step 3: Build and Install Kia

```bash
# Navigate to Kia source directory
cd /path/to/kia

# Build Kia
make clean
make

# Install Kia (this will install all necessary files)
sudo make install
```

#### Step 4: Configure Kia

```bash
# Edit configuration file
sudo nano /etc/kia/config

# Set your preferred default session
# Example: default_session=xfce
# Or: default_session=gnome
# Or: default_session=kde

# Verify PAM configuration
cat /etc/pam.d/kia
```

#### Step 5: Disable Current Display Manager

**IMPORTANT**: Do this carefully to avoid being locked out.

```bash
# For GDM
sudo systemctl stop gdm
sudo systemctl disable gdm

# For LightDM
sudo systemctl stop lightdm
sudo systemctl disable lightdm

# For SDDM
sudo systemctl stop sddm
sudo systemctl disable sddm

# For LXDM
sudo systemctl stop lxdm
sudo systemctl disable lxdm

# Verify it's disabled
systemctl is-enabled gdm lightdm sddm lxdm 2>/dev/null
```

**Optional but Recommended**: Mask the old display manager to prevent accidental activation:
```bash
sudo systemctl mask gdm  # or lightdm, sddm, etc.
```

#### Step 6: Enable Kia

```bash
# Enable Kia service
sudo systemctl enable kia

# Verify it's enabled
systemctl is-enabled kia

# Check for conflicts
systemctl list-dependencies graphical.target | grep -E 'gdm|lightdm|sddm|kia'
```

#### Step 7: Test Without Rebooting (Optional but Recommended)

```bash
# Switch to a different TTY first (Ctrl+Alt+F2)
# Then start Kia manually to test
sudo systemctl start kia

# Switch to TTY7 (Ctrl+Alt+F7) to see Kia
# If it works, you can proceed to reboot
# If not, switch back to TTY2 and troubleshoot
```

#### Step 8: Reboot

```bash
# Reboot to start Kia automatically
sudo reboot
```

### Post-Migration Verification

After rebooting, verify Kia is working correctly:

1. **Check Kia is running**:
   ```bash
   systemctl status kia
   ```

2. **Verify no other display managers are running**:
   ```bash
   ps aux | grep -E 'gdm|lightdm|sddm' | grep -v grep
   ```

3. **Check logs for errors**:
   ```bash
   sudo journalctl -u kia -n 50
   sudo tail /var/log/kia.log
   ```

4. **Test login functionality**:
   - Try logging in with your user account
   - Verify session selection works
   - Check that your desktop environment launches correctly

### Reverting to Previous Display Manager

If you need to revert to your previous display manager:

#### Quick Revert

```bash
# Disable Kia
sudo systemctl stop kia
sudo systemctl disable kia

# Re-enable previous display manager (example: GDM)
sudo systemctl unmask gdm  # If you masked it
sudo systemctl enable gdm
sudo systemctl start gdm

# Or reboot
sudo reboot
```

#### Complete Revert

```bash
# Stop Kia
sudo systemctl stop kia
sudo systemctl disable kia
sudo systemctl mask kia  # Prevent accidental start

# Restore previous display manager
sudo systemctl unmask gdm  # Replace 'gdm' with your DM
sudo systemctl enable gdm
sudo systemctl start gdm

# Restore backed-up configuration (if needed)
sudo cp -r /root/dm-backup/gdm* /etc/ 2>/dev/null

# Reboot
sudo reboot
```

### Migration Tips by Distribution

#### Debian/Ubuntu

```bash
# GDM is common on Ubuntu with GNOME
# LightDM is common on Ubuntu with other DEs

# After migration, you may want to remove the old DM
sudo apt-get remove --purge gdm3  # Be careful with this
# Or
sudo apt-get remove --purge lightdm
```

#### Arch Linux

```bash
# Check current DM
cat /etc/systemd/system/display-manager.service

# After migration, optionally remove old DM
sudo pacman -Rns gdm  # Be careful with this
# Or
sudo pacman -Rns lightdm
```

#### Fedora

```bash
# GDM is default on Fedora
# After migration, optionally remove old DM
sudo dnf remove gdm  # Be careful with this
```

### Common Migration Issues

#### Issue: Black screen after reboot

**Solution**:
1. Switch to TTY2 (Ctrl+Alt+F2)
2. Login as root or your user
3. Check Kia status: `sudo systemctl status kia`
4. Check logs: `sudo journalctl -u kia -n 50`
5. If Kia failed to start, revert to previous DM

#### Issue: Both Kia and old DM try to start

**Solution**:
```bash
# Ensure old DM is fully disabled
sudo systemctl disable --now gdm lightdm sddm
sudo systemctl mask gdm lightdm sddm
sudo systemctl restart kia
```

#### Issue: Sessions don't appear in Kia

**Solution**:
```bash
# Verify session files exist
ls /usr/share/xsessions/
ls /usr/share/wayland-sessions/

# If missing, reinstall your desktop environment
# Example for XFCE on Debian:
sudo apt-get install --reinstall xfce4
```

#### Issue: Can't login after migration

**Solution**:
1. Switch to TTY2 (Ctrl+Alt+F2)
2. Login directly
3. Check PAM configuration: `cat /etc/pam.d/kia`
4. Test authentication: `su - yourusername`
5. Check Kia logs: `sudo tail /var/log/kia.log`

### Testing in a Virtual Machine

Before migrating your main system, consider testing Kia in a virtual machine:

```bash
# Install VirtualBox or QEMU
# Create a VM with your distribution
# Install a desktop environment
# Follow the migration steps above
# Test thoroughly before migrating your main system
```

This allows you to safely test the migration process and familiarize yourself with Kia before committing to the change on your primary system.

## License

MIT License

Copyright (c) 2025 Kia Display Manager Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Development

### Project Structure
```
kia/
├── src/          # Source files
├── include/      # Header files
├── tests/        # Unit and integration tests
├── config/       # Configuration templates
├── docs/         # Documentation
└── build/        # Build artifacts (generated)
```

### Building Tests
```bash
make test
```

## Contributing

Contributions are welcome! Please ensure:
- Code follows C11 standard
- All tests pass
- Security best practices are followed
- Documentation is updated

## Support

For issues and questions, please check:
- System logs: `journalctl -u kia`
- Application logs: `/var/log/kia.log`
- PAM logs: `journalctl -t kia`
