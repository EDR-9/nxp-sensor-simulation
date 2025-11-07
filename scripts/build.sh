#!/bin/bash

# Compiling kernel module and CLI

# Exit oncritical errors
set -e errexit
set -e nounset
set -e pipefail


# Binaries from compilation
KERNEL_BUILD=$(find kernel -name '*.ko' -print -quit)  #"~/workdir/simtemp/kernel/nxp_simtemp.ko"
CLI_BUILD="./user/cli/usr_interface"  #~/workdir/simtemp/user/cli/usr_interface"

# Check if kernel module exists
if [[ -n "$KERNEL_BUILD" ]]; then
    echo "Kernel module already built: $KERNEL_BUILD"
else
    echo "$KERNEL_BUILD binary not found. Building kernel module..."
    make -C kernel || { echo "kernel compilation failed"; exit 1; }
fi

# Check if CLI binary exists
if [[ -x "$CLI_BUILD" ]]; then
    echo "CLI already built: $CLI_BUILD"
else
    echo "$CLI_BUILD not found. Building CLI..."
    g++ "$CLI_BUILD.cpp" -o $CLI_BUILD || { echo "CLI compilation failed"; exit 1; }
fi

echo "Compilation process completed"