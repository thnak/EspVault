#!/bin/bash
#
# QEMU Test Runner for EspVault
# 
# This script builds and runs QEMU tests for EspVault provisioning functionality.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     EspVault QEMU Test Runner                      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo -e "${RED}Error: ESP-IDF environment not set up${NC}"
    echo -e "${YELLOW}Please run: source \$IDF_PATH/export.sh${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} ESP-IDF found: $IDF_PATH"
echo ""

# Navigate to test directory
cd "$SCRIPT_DIR"

# Clean previous build (optional)
if [ "$1" == "--clean" ] || [ "$1" == "-c" ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    idf.py fullclean
    echo -e "${GREEN}✓${NC} Clean complete"
    echo ""
fi

# Build the test firmware
echo -e "${BLUE}Building test firmware...${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if idf.py build; then
    echo ""
    echo -e "${GREEN}✓${NC} Build successful"
else
    echo ""
    echo -e "${RED}✗${NC} Build failed"
    exit 1
fi

echo ""

# Check if QEMU is available
if ! command -v qemu-system-xtensa &> /dev/null; then
    echo -e "${YELLOW}Warning: QEMU for Xtensa not found${NC}"
    echo -e "${YELLOW}Install with: \$IDF_PATH/install.sh --enable-qemu${NC}"
    echo ""
    echo -e "${YELLOW}Attempting to use idf.py qemu instead...${NC}"
fi

# Run tests in QEMU
echo -e "${BLUE}Running tests in QEMU...${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Create a temporary file for QEMU output
QEMU_OUTPUT=$(mktemp)

# Run QEMU with timeout
# Note: QEMU will exit when the test completes
timeout 60 idf.py qemu monitor 2>&1 | tee "$QEMU_OUTPUT" || {
    QEMU_EXIT_CODE=$?
    if [ $QEMU_EXIT_CODE -eq 124 ]; then
        echo ""
        echo -e "${RED}✗${NC} Tests timed out after 60 seconds"
        rm -f "$QEMU_OUTPUT"
        exit 1
    fi
}

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Parse test results
if grep -q "All Tests Completed" "$QEMU_OUTPUT"; then
    echo -e "${GREEN}✓${NC} Test suite completed"
    
    # Count passed/failed tests
    if grep -q "Tests.*Failures.*Ignored" "$QEMU_OUTPUT"; then
        TEST_SUMMARY=$(grep -o "[0-9]* Tests [0-9]* Failures [0-9]* Ignored" "$QEMU_OUTPUT" | tail -1)
        echo -e "${GREEN}✓${NC} $TEST_SUMMARY"
        
        # Check if any tests failed
        FAILURES=$(echo "$TEST_SUMMARY" | grep -o "[0-9]* Failures" | grep -o "[0-9]*")
        if [ "$FAILURES" != "0" ]; then
            echo ""
            echo -e "${RED}✗${NC} Some tests failed"
            rm -f "$QEMU_OUTPUT"
            exit 1
        fi
    fi
    
    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║          All Tests Passed Successfully!            ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════╝${NC}"
    
    rm -f "$QEMU_OUTPUT"
    exit 0
else
    echo -e "${RED}✗${NC} Tests did not complete successfully"
    echo ""
    echo -e "${YELLOW}Last 20 lines of output:${NC}"
    tail -20 "$QEMU_OUTPUT"
    
    rm -f "$QEMU_OUTPUT"
    exit 1
fi
