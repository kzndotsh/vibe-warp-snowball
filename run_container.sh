#!/bin/bash

# Script to run the Blue Snowball driver development container

echo "Starting Blue Snowball driver development container..."

# Run the container with necessary privileges and volume mounts
docker run --rm -it \
    --privileged \
    --cap-add=SYS_MODULE \
    --cap-add=SYS_ADMIN \
    -v /lib/modules:/lib/modules:ro \
    -v /usr/src:/usr/src:ro \
    -v /dev/bus/usb:/dev/bus/usb \
    -v /proc/modules:/proc/modules:ro \
    -v /sys/module:/sys/module:ro \
    --name blue-snowball-dev \
    blue-snowball-driver \
    /bin/bash -c "
        export KERNEL_VERSION=\$(uname -r)
        export KDIR=/lib/modules/\$(uname -r)/build
        echo 'Blue Snowball Driver Development Environment'
        echo 'Kernel Version: '\$KERNEL_VERSION
        echo 'Kernel Directory: '\$KDIR
        echo ''
        echo 'Available commands:'
        echo '  make         - Build the kernel module'
        echo '  make load    - Load the module'
        echo '  make unload  - Unload the module'
        echo '  make test    - Run the test suite'
        echo '  make clean   - Clean build artifacts'
        echo ''
        /bin/bash
    "
