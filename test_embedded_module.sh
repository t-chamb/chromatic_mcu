#!/bin/bash
# Test script for embedded module loading

echo "Testing embedded module loading..."
echo ""
echo "Commands to run in monitor:"
echo "1. modloadem hello    - Load the embedded hello module"
echo "2. modlist            - List loaded modules"
echo "3. modstats           - Show memory statistics"
echo "4. modunload Hello    - Unload the module"
echo ""
echo "Starting monitor..."
echo ""

idf.py monitor
