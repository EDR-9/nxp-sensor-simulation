#!/bin/bash
set -e


### MODULE LOADING

MODULE=nxp_simtemp
DEVICE=/dev/nxp_simtemp
SYSFSPATH=/sys/devices/platform/nxp_simtemp
TIMEOUT=5
MESSAGES=20

# Unload if already loaded
if lsmod | grep -q "$MODULE"; then
    echo "Removing current module..."
    sudo rmmod $MODULE || echo "Module was busy, retrying..."
    sleep 1
fi

echo "Loading $MODULE kernel module..."
sudo insmod ./kernel/$MODULE.ko

# Check if device node exists
if [ -e $DEVICE ]; then
    echo "Device $DEVICE is ready"
else
    echo "Device not found. Module loading failed"
    exit 1
fi

# Show kernel messages
echo
sudo dmesg | tail -n $MESSAGES


### PARAMETER CONFIGURATION (READING AND WRITING)

# Using terminal commands
echo
echo "Reading default sensor parameters"

echo
echo "sampling"
cat "$SYSFSPATH/sampling"

echo
echo "threshold"
cat "$SYSFSPATH/threshold"

echo
echo "mode"
cat "$SYSFSPATH/mode"

echo
echo "stats"
cat "$SYSFSPATH/stats"

echo
echo "Changing initial parameters"

echo
sudo sh -c "echo 1000 > $SYSFSPATH/sampling"
echo "sampling new value"
cat "$SYSFSPATH/sampling"

echo
sudo sh -c "echo 40000 > $SYSFSPATH/threshold"
echo "threshold new value"
cat "$SYSFSPATH/threshold"

echo
sudo sh -c "echo noisy > $SYSFSPATH/mode"
echo "mode new value"
cat "$SYSFSPATH/mode"

echo
echo "Module loaded and configured"

# Chek again if device still exist
if [ ! -e "$DEVICE" ]; then
    echo "Device $DEVICE not found. Load the module first."
    exit 1
fi

echo
echo "Reading from $DEVICE"
sudo timeout $TIMEOUT cat $DEVICE | hexdump -C
echo
echo "Read test completed"

echo
echo "Unloading $MODULE module..."
# sudo rmmod simtemp
# echo "Module removed."

echo "Checking if $MODULE is loaded..."
if lsmod | grep -q "$MODULE"; then
    echo "Attempting to remove module..."
    sudo rmmod $MODULE || {
        echo "Normal removal failed. Trying force cleanup..."
        sudo fuser -km $DEVICE || true
        sudo rmmod $MODULE || echo "Could not unload module"
    }
else
    echo "Module not loaded."
fi

sleep 1
echo
sudo dmesg | tail -n $MESSAGES