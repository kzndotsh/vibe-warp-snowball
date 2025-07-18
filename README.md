# Blue Snowball USB Audio Driver

A Linux kernel module for the Blue Snowball USB microphone (VID: 0x0d8c, PID: 0x0013).

## Overview

This project provides a custom kernel driver for the Blue Snowball USB microphone that integrates with the ALSA (Advanced Linux Sound Architecture) subsystem. The driver handles USB communication and audio data capture from the microphone.

## Features

- Full ALSA integration
- USB device detection and management
- Audio capture support (16-bit, 44.1kHz, mono)
- Power management (suspend/resume)
- Error handling and cleanup
- Docker-based sandboxed development environment

## Project Structure

```
├── blue_snowball.c       # Main driver source code
├── Makefile             # Standard Makefile for kernel module
├── Makefile.flexible    # Flexible Makefile with kernel version detection
├── Dockerfile           # Docker container for development
├── setup_dev_env.sh     # Development environment setup script
├── run_container.sh     # Container runner script
├── run_simple.sh        # Simplified container runner
├── test_suite.sh        # Basic test suite
├── test_mock_device.py  # Comprehensive Python test suite
└── README.md           # This file
```

## Requirements

### Host System
- Linux kernel 6.x (tested with 6.15.x)
- Kernel headers for your running kernel
- Build tools (gcc, make)
- Docker (for containerized development)

### Kernel Headers Installation
```bash
# Arch Linux
sudo pacman -S linux-zen-headers base-devel

# Ubuntu/Debian
sudo apt-get install linux-headers-$(uname -r) build-essential

# Fedora
sudo dnf install kernel-headers kernel-devel
```

## Building the Driver

### Method 1: Direct Build
```bash
# Install kernel headers first
sudo ./setup_dev_env.sh

# Build the module
make -f Makefile.flexible all
```

### Method 2: Docker Build
```bash
# Build the container
docker build -t blue-snowball-driver .

# Run the container
./run_simple.sh

# Inside container
make -f Makefile.flexible all
```

## Installation and Usage

### Load the Module
```bash
sudo insmod blue_snowball.ko
```

### Verify Installation
```bash
# Check if module is loaded
lsmod | grep blue_snowball

# Check kernel messages
dmesg | grep -i "blue snowball"

# Check ALSA devices
arecord -l
```

### Test Audio Capture
```bash
# Record 5 seconds of audio
arecord -D plughw:CARD=S,DEV=0 -f cd -t wav -d 5 test.wav

# Play back the recording
aplay test.wav
```

### Unload the Module
```bash
sudo rmmod blue_snowball
```

## Testing

### Automated Test Suite
```bash
# Run comprehensive tests
python3 test_mock_device.py

# Run basic tests
./test_suite.sh
```

### Manual Testing
```bash
# Build and test
make -f Makefile.flexible clean
make -f Makefile.flexible all
modinfo blue_snowball.ko
```

## Driver Architecture

### Core Components

1. **USB Driver Registration**
   - Registers with the USB subsystem
   - Handles device detection and removal
   - Manages USB endpoints

2. **ALSA Integration**
   - Creates ALSA sound cards
   - Implements PCM operations
   - Handles audio buffer management

3. **Audio Processing**
   - USB Request Blocks (URBs) for data transfer
   - Audio format conversion
   - Buffer management

### Key Functions

- `blue_snowball_probe()` - Device detection and initialization
- `blue_snowball_disconnect()` - Device removal and cleanup
- `blue_snowball_pcm_*()` - PCM audio operations
- `blue_snowball_urb_complete()` - USB data transfer completion

## Technical Specifications

- **Vendor ID**: 0x0d8c
- **Product ID**: 0x0013
- **Audio Format**: 16-bit PCM, 44.1kHz, Mono
- **Transfer Type**: USB Bulk
- **Buffer Size**: 88,200 bytes (1 second of audio)
- **Period Size**: 1024-22050 bytes

## Development Environment

### Docker Setup
The project includes a Docker-based development environment that provides:
- Ubuntu 22.04 base
- Kernel development tools
- ALSA utilities
- USB debugging tools

### Safety Features
- Sandboxed development environment
- Comprehensive error handling
- Memory leak prevention
- Proper resource cleanup

## Troubleshooting

### Common Issues

1. **Module won't load**
   - Check kernel version compatibility
   - Verify kernel headers are installed
   - Check dmesg for error messages

2. **Device not recognized**
   - Verify USB device ID matches
   - Check if device is already claimed by another driver
   - Use `lsusb` to verify device presence

3. **Audio capture fails**
   - Check ALSA device permissions
   - Verify audio format settings
   - Check USB endpoint configuration

### Debug Commands
```bash
# Check USB devices
lsusb -v | grep -A 10 -B 10 "0d8c"

# Check kernel messages
dmesg | tail -50

# Check ALSA devices
cat /proc/asound/cards

# Check module dependencies
modinfo -F depends blue_snowball.ko
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run the test suite
5. Submit a pull request

## License

This project is licensed under the GPL v2 license, compatible with the Linux kernel.

## Disclaimer

This driver is provided as-is for educational and development purposes. Use at your own risk. Always test in a safe environment before deploying to production systems.

## References

- [Linux USB Driver Development](https://www.kernel.org/doc/html/latest/driver-api/usb/index.html)
- [ALSA Driver Development](https://www.kernel.org/doc/html/latest/sound/kernel-api/index.html)
- [Blue Snowball Specifications](https://www.bluemic.com/en-us/products/snowball/)
