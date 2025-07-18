#!/bin/bash

# Setup script for Blue Snowball kernel driver development

set -e

echo "Setting up Blue Snowball kernel driver development environment..."

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "This script needs to be run as root (sudo)"
    exit 1
fi

# Get current kernel version
KERNEL_VERSION=$(uname -r)
echo "Current kernel version: $KERNEL_VERSION"

# Install kernel headers on Arch Linux
echo "Installing kernel headers..."
if command -v pacman &> /dev/null; then
    # Arch Linux
    pacman -S --noconfirm linux-zen-headers base-devel
elif command -v apt-get &> /dev/null; then
    # Ubuntu/Debian
    apt-get update
    apt-get install -y linux-headers-$(uname -r) build-essential
elif command -v dnf &> /dev/null; then
    # Fedora
    dnf install -y kernel-headers kernel-devel
else
    echo "Unsupported package manager. Please install kernel headers manually."
    exit 1
fi

# Verify kernel headers installation
KDIR="/lib/modules/$KERNEL_VERSION/build"
if [ -d "$KDIR" ]; then
    echo "Kernel headers installed successfully at: $KDIR"
else
    echo "Failed to install kernel headers. Directory $KDIR not found."
    exit 1
fi

# Install additional development tools
echo "Installing additional development tools..."
if command -v pacman &> /dev/null; then
    pacman -S --noconfirm alsa-utils usbutils
elif command -v apt-get &> /dev/null; then
    apt-get install -y alsa-utils usbutils
fi

echo "Development environment setup complete!"
echo "You can now run 'make' to build the kernel module."
