#!/bin/bash

# ==============================================================================
# Comprehensive Automated Test Suite for Multi-Threaded Race Simulation
# Uses fast custom track: ../data/circle
# ==============================================================================

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

LOG_DIR="test_logs"
EXECUTABLE="./race"
CIRCLE_MAP="../data/circle"

# Проверка наличия бинарника
if [ ! -f "$EXECUTABLE" ]; then
    if [ -f "./bin/race" ]; then
        EXECUTABLE="./bin/race"
    else
        echo -e "${RED}[ERROR] Executable './race' not found! Please build the project first (make).${NC}"
        exit 1
    fi
fi

mkdir -p "$LOG_DIR"

PASSED=0
FAILED=0
TOTAL=0

run_test() {
    local test_num=$1
    local test_name=$2
    local cmd=$3
    local expect_success=$4 # 1 for success (exit 0), 0 for failure (exit non-zero)
    local check_file=$5     # optional file to check if it exists and non-empty

    TOTAL=$((TOTAL + 1))
    echo -n -e "${BLUE}[TEST $test_num]${NC} $test_name... "

    # Запускаем команду и скрываем вывод в консоли
    eval "$cmd" > /dev/null 2>&1
    local status=$?

    local ok=1
    if [ "$expect_success" -eq 1 ] && [ $status -ne 0 ]; then
        ok=0
    elif [ "$expect_success" -eq 0 ] && [ $status -eq 0 ]; then
        ok=0
    fi

    # Если указан файл для проверки, убедимся что он создан и не пустой
    if [ -n "$check_file" ]; then
        if [ ! -f "$check_file" ] || [ ! -s "$check_file" ]; then
            ok=0
        fi
    fi

    if [ $ok -eq 1 ]; then
        echo -e "${GREEN}PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}"
        FAILED=$((FAILED + 1))
    fi
}

echo "=================================================="
echo "      STARTING EXTENDED RACE TEST SUITE           "
echo "=================================================="
echo ""

# ------------------------------------------------------------------------------
# Test 1: Help message validation
# ------------------------------------------------------------------------------
run_test 1 "Testing --help flag output" "$EXECUTABLE --help" 1 ""

# ------------------------------------------------------------------------------
# Test 2: Default run with fast circle map
# ------------------------------------------------------------------------------
run_test 2 "Running default simulation on circle map" "$EXECUTABLE -m $CIRCLE_MAP -d 1" 1 ""

# ------------------------------------------------------------------------------
# Test 3: Custom Laps with circle map and log output
# ------------------------------------------------------------------------------
LOG3="$LOG_DIR/test3_laps.log"
run_test 3 "Custom laps (-l 3) with log output" "$EXECUTABLE -m $CIRCLE_MAP -l 3 -d 1 -o $LOG3" 1 "$LOG3"

# ------------------------------------------------------------------------------
# Test 4: Custom Teams, Drivers and Start Grid
# ------------------------------------------------------------------------------
LOG4="$LOG_DIR/test4_teams.log"
run_test 4 "Custom teams (-t) & custom start grid (-s)" "$EXECUTABLE -m $CIRCLE_MAP -t \"TeamA:red:Alice,Bob\" -t \"TeamB:blue:Charlie,Dave\" -s \"Alice,Charlie,Bob,Dave\" -l 1 -d 1 -o $LOG4" 1 "$LOG4"

# ------------------------------------------------------------------------------
# Test 5: Minimal draw delay stress test (Fast run)
# ------------------------------------------------------------------------------
LOG5="$LOG_DIR/test5_speed.log"
run_test 5 "Minimal draw delay (-d 1) stress test" "$EXECUTABLE -m $CIRCLE_MAP -l 3 -d 1 -o $LOG5" 1 "$LOG5"

# ------------------------------------------------------------------------------
# Test 6: Graceful Shutdown (SIGINT / Ctrl+C simulation)
# ------------------------------------------------------------------------------
LOG6="$LOG_DIR/test6_sigint.log"
echo -n -e "${BLUE}[TEST 6]${NC} Testing SIGINT (Ctrl+C) graceful shutdown & log save... "
TOTAL=$((TOTAL + 1))

# Запуск долгой симуляции в фоне
$EXECUTABLE -m "$CIRCLE_MAP" -l 20 -d 100 -o "$LOG6" > /dev/null 2>&1 &
BG_PID=$!

sleep 0.8
kill -INT $BG_PID
wait $BG_PID 2>/dev/null

if [ -f "$LOG6" ]; then
    echo -e "${GREEN}PASSED${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}FAILED${NC}"
    FAILED=$((FAILED + 1))
fi

# ------------------------------------------------------------------------------
# Test 7: Robustness - Invalid negative laps
# ------------------------------------------------------------------------------
run_test 7 "Rejecting negative laps (-l -3)" "$EXECUTABLE -m $CIRCLE_MAP -l -3" 0 ""

# ------------------------------------------------------------------------------
# Test 8: Robustness - Non-existent map file
# ------------------------------------------------------------------------------
run_test 8 "Handling non-existent map file path" "$EXECUTABLE -m data/non_existent_map_file" 0 ""

# ------------------------------------------------------------------------------
# Summary Report
# ------------------------------------------------------------------------------
echo ""
echo "=================================================="
echo "                  TEST SUMMARY                    "
echo "=================================================="
echo -e "Total tests run: $TOTAL"
echo -e "Passed:          ${GREEN}$PASSED${NC}"
echo -e "Failed:          ${RED}$FAILED${NC}"
echo "--------------------------------------------------"
echo "All generated test logs are safely stored in: ./$LOG_DIR/"
echo "=================================================="

if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
