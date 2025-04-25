#!/usr/bin/bash

# This script runs the mspgcc-elf-addr2line executable to convert an address
# to a human-readable line number of a file given an executable file

EXECUTABLE_FILE="$1" # First argument: path to executable file
PC_ADDRESS="$2"      # Second argument: program counter address

if [ -z "$EXECUTABLE_FILE" ] || [ -z "$PC_ADDRESS" ]; then
        echo "Usage: $0 <executable-file> <pc-address>"
        exit 1
fi

msp430-elf-addr2line -e "$EXECUTABLE_FILE" -f -C "$PC_ADDRESS"





