FROM ubuntu:22.04

# Install kernel development tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    linux-headers-generic \
    kmod \
    git \
    vim \
    gdb \
    strace \
    usbutils \
    alsa-utils \
    pulseaudio \
    pulseaudio-utils \
    python3 \
    python3-pip \
    curl \
    wget \
    pkg-config \
    libusb-1.0-0-dev \
    libasound2-dev \
    && rm -rf /var/lib/apt/lists/*

# Create working directory
WORKDIR /kernel-dev

# Copy source files
COPY . .

# Set environment variables
# Note: KERNEL_VERSION and KDIR will be set at runtime in the container

CMD ["/bin/bash"]
