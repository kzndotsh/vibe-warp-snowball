#!/bin/bash

echo "Starting Blue Snowball driver development container..."

# Create a simple container for kernel development
docker run --rm -it \
    --privileged \
    -v /lib/modules:/lib/modules:ro \
    -w /kernel-dev \
    blue-snowball-driver \
    bash
