#!/bin/bash

# Launch LVGL Simulator
# Run from dev_tools/ directory

set -e

# Ensure we're in dev_tools
if [ ! -f "chromatic-api-server.js" ]; then
    echo "Error: Must run from dev_tools/ directory"
    exit 1
fi

# Check if simulator is set up
if [ ! -d "lvgl_simulator" ]; then
    echo "Error: LVGL simulator not set up"
    echo "Run: ./setup_lvgl_simulator.sh"
    exit 1
fi

cd lvgl_simulator/build

# Build if needed
if [ ! -f "Makefile" ]; then
    echo "Configuring build..."
    cmake ..
fi

echo "Building simulator..."
make

echo ""
echo "Launching LVGL simulator..."
../bin/main
