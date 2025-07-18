#!/bin/bash

# Test suite for Blue Snowball kernel module

# Load the module
insmod blue_snowball.ko
if [ $? -ne 0 ]; then
  echo "Failed to load Blue Snowball module"
  exit 1
fi

echo "Module loaded successfully"

# Check if the device is recognized
lsusb | grep "Blue Microphones"
if [ $? -ne 0 ]; then
  echo "Blue Snowball device not found"
  exit 1
fi

echo "Blue Snowball device found"

# Check ALSA device
arecord -l | grep "Blue Snowball"
if [ $? -ne 0 ]; then
  echo "ALSA device for Blue Snowball not found"
  exit 1
fi

echo "ALSA device for Blue Snowball found"

# Run a test recording
arecord -D plughw:CARD=S,DEV=0 -f cd -t wav -d 5 test.wav
if [ $? -ne 0 ]; then
  echo "Failed to record from Blue Snowball"
  exit 1
fi

echo "Recording from Blue Snowball successful"

# Unload the module
rmmod blue_snowball
if [ $? -ne 0 ]; then
  echo "Failed to unload Blue Snowball module"
  exit 1
fi

echo "Module unloaded successfully"

exit 0
