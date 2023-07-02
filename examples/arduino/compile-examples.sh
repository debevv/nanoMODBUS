#!/bin/env bash
set -e -o pipefail

# Set default board
BOARD=$1
if [ -z "${BOARD}" ]
then
    echo No board specified, using arduino:avr:uno
    BOARD=arduino:avr:uno
fi

# Set up example folders
rm -rf build/arduino
mkdir -p build/arduino
cp -r examples/arduino build
cp nanomodbus.h nanomodbus.c build/arduino/server-rtu/
cp nanomodbus.h nanomodbus.c build/arduino/client-rtu/

# Ensure ardunio-cli is up to date
arduino-cli core update-index
arduino-cli core install arduino:avr

# Compile both examples
arduino-cli compile --clean -b $BOARD --output-dir build/arduino -- build/arduino/server-rtu
arduino-cli compile --clean -b $BOARD --output-dir build/arduino -- build/arduino/client-rtu

