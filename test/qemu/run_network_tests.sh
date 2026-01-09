#!/usr/bin/env bash
# QEMU Network Testing Setup Script
#
# This script sets up and runs QEMU tests with network capabilities,
# including Ethernet interface and MQTT broker connectivity.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BROKER_PORT=1883
BROKER_PID_FILE="/tmp/mosquitto_qemu_test.pid"
TEST_TIMEOUT=180

echo -e "${BLUE}╔══════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   EspVault QEMU Network Test Suite          ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════╝${NC}"
echo ""

# Function to check if mosquitto is installed
check_mosquitto() {
    if ! command -v mosquitto &> /dev/null; then
        echo -e "${RED}Error: mosquitto is not installed${NC}"
        echo "Please install it:"
        echo "  Ubuntu/Debian: sudo apt-get install mosquitto mosquitto-clients"
        echo "  macOS: brew install mosquitto"
        echo "  Windows: Download from https://mosquitto.org/download/"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} mosquitto found"
}

# Function to start MQTT broker
start_broker() {
    echo ""
    echo -e "${YELLOW}Starting MQTT broker...${NC}"
    
    # Check if broker is already running
    if [ -f "$BROKER_PID_FILE" ]; then
        OLD_PID=$(cat "$BROKER_PID_FILE")
        if kill -0 "$OLD_PID" 2>/dev/null; then
            echo -e "${YELLOW}Broker already running (PID: $OLD_PID)${NC}"
            return
        fi
    fi
    
    # Start broker in background
    mosquitto -v -p $BROKER_PORT &
    BROKER_PID=$!
    echo $BROKER_PID > "$BROKER_PID_FILE"
    
    echo -e "${GREEN}✓${NC} MQTT broker started (PID: $BROKER_PID)"
    echo "  Listening on: localhost:$BROKER_PORT"
    echo "  Accessible from QEMU at: 10.0.2.2:$BROKER_PORT"
    
    # Wait for broker to be ready
    sleep 2
}

# Function to stop MQTT broker
stop_broker() {
    if [ -f "$BROKER_PID_FILE" ]; then
        BROKER_PID=$(cat "$BROKER_PID_FILE")
        if kill -0 "$BROKER_PID" 2>/dev/null; then
            echo ""
            echo -e "${YELLOW}Stopping MQTT broker (PID: $BROKER_PID)...${NC}"
            kill "$BROKER_PID"
            rm -f "$BROKER_PID_FILE"
            echo -e "${GREEN}✓${NC} Broker stopped"
        fi
    fi
}

# Function to subscribe to test topics
monitor_mqtt() {
    echo ""
    echo -e "${YELLOW}Monitoring MQTT topics...${NC}"
    echo "  Subscribed to: dev/#"
    echo "  Press Ctrl+C to stop monitoring"
    echo ""
    mosquitto_sub -h localhost -p $BROKER_PORT -t "dev/#" -v &
    MONITOR_PID=$!
    echo $MONITOR_PID
}

# Function to build test firmware
build_tests() {
    echo ""
    echo -e "${YELLOW}Building test firmware...${NC}"
    
    if [ "$1" == "--clean" ]; then
        echo "Cleaning build directory..."
        idf.py fullclean
    fi
    
    idf.py build
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} Build successful"
    else
        echo -e "${RED}✗${NC} Build failed"
        exit 1
    fi
}

# Function to run QEMU tests
run_qemu_tests() {
    echo ""
    echo -e "${YELLOW}Running QEMU tests with network support...${NC}"
    echo "  Network mode: User-mode (SLIRP)"
    echo "  Guest IP: 10.0.2.15"
    echo "  Gateway: 10.0.2.2"
    echo "  Broker accessible at: 10.0.2.2:$BROKER_PORT"
    echo ""
    
    # Run QEMU with network support
    # Note: idf.py qemu already configures user-mode networking
    timeout $TEST_TIMEOUT idf.py qemu monitor || {
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            echo -e "${RED}✗${NC} Test timeout after ${TEST_TIMEOUT}s"
        else
            echo -e "${RED}✗${NC} Test failed with exit code $EXIT_CODE"
        fi
        return $EXIT_CODE
    }
    
    echo -e "${GREEN}✓${NC} Tests completed"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --clean          Clean build before running tests"
    echo "  --broker-only    Only start MQTT broker (don't run tests)"
    echo "  --stop-broker    Stop running MQTT broker"
    echo "  --monitor        Monitor MQTT traffic"
    echo "  --help           Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0               # Run tests with network support"
    echo "  $0 --clean       # Clean build and run tests"
    echo "  $0 --broker-only # Start broker and wait"
    echo "  $0 --monitor     # Monitor MQTT traffic"
    echo ""
}

# Parse command line arguments
CLEAN_BUILD=false
BROKER_ONLY=false
MONITOR_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --broker-only)
            BROKER_ONLY=true
            shift
            ;;
        --stop-broker)
            stop_broker
            exit 0
            ;;
        --monitor)
            MONITOR_ONLY=true
            shift
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
check_mosquitto

if [ "$MONITOR_ONLY" == true ]; then
    MONITOR_PID=$(monitor_mqtt)
    wait $MONITOR_PID
    exit 0
fi

start_broker

# Trap to ensure broker is stopped on exit
trap stop_broker EXIT

if [ "$BROKER_ONLY" == true ]; then
    echo ""
    echo -e "${GREEN}MQTT broker is running${NC}"
    echo "Press Ctrl+C to stop"
    echo ""
    echo "Test from another terminal:"
    echo "  mosquitto_pub -h localhost -t test/topic -m 'hello'"
    echo "  mosquitto_sub -h localhost -t '#'"
    echo ""
    wait
else
    if [ "$CLEAN_BUILD" == true ]; then
        build_tests --clean
    else
        build_tests
    fi
    
    run_qemu_tests
fi

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Network Test Suite Completed               ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════╝${NC}"
