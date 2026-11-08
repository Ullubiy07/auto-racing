#!/bin/bash

# Tests for independently launched race and car processes on Linux.
set -u
set -o pipefail

PROJECT_DIR=${PROJECT_DIR:-$(CDPATH= cd "$(dirname "$0")" && pwd)}
RACE=${RACE_EXECUTABLE:-"$PROJECT_DIR/bin/race"}
CAR=${CAR_EXECUTABLE:-"$PROJECT_DIR/bin/car"}
CIRCLE_MAP="$PROJECT_DIR/data/circle"
TEST_TIMEOUT=${TEST_TIMEOUT:-20}
LOG_ROOT=${LOG_DIR:-"$PROJECT_DIR/bin/test_logs"}

for executable in "$RACE" "$CAR"; do
    if [[ ! -x "$executable" ]]; then
        echo "Executable not found: $executable. Build both race and car first." >&2
        exit 1
    fi
done
for command in timeout grep awk; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Required command not found: $command" >&2
        exit 1
    fi
done
if [[ ! "$TEST_TIMEOUT" =~ ^[1-9][0-9]*$ ]]; then
    echo "TEST_TIMEOUT must be a positive integer" >&2
    exit 1
fi
mkdir -p "$LOG_ROOT" || exit 1
LOG_DIR=$(mktemp -d "$LOG_ROOT/run.XXXXXX") || exit 1

PASSED=0
FAILED=0
TOTAL=0
ACTIVE_PIDS=()
RACE_PID=""
CAR_PIDS=()
REPORT=""
TEST_DIR=""

# Only remove IPC objects belonging to the race started by this test.
ipc_files() {
    [[ -n "$RACE_PID" ]] || return 0
    local file
    for file in /dev/shm/race-shm-"$RACE_PID" \
                /dev/shm/track-shm-"$RACE_PID" \
                /dev/shm/sem.cell-"$RACE_PID"-* \
                /dev/shm/sem.race-"$RACE_PID"-car-* \
                /dev/shm/sem.race-cars-ready-"$RACE_PID" \
                /dev/shm/sem.race-cars-stopped-"$RACE_PID" \
                /dev/shm/sem.race-join-"$RACE_PID"; do
        [[ ! -e "$file" ]] || printf '%s\n' "$file"
    done
}

cleanup() {
    local pid
    for pid in "${ACTIVE_PIDS[@]}"; do
        kill -TERM "$pid" 2>/dev/null || true
    done
    if (( ${#ACTIVE_PIDS[@]} )); then
        sleep 0.1
    fi
    for pid in "${ACTIVE_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
        wait "$pid" 2>/dev/null || true
    done
    ACTIVE_PIDS=()
    # After a failed test, remove only this test's leftover IPC objects.
    local file
    while IFS= read -r file; do
        rm -f -- "$file"
    done < <(ipc_files)
    RACE_PID=""
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

wait_until() {
    local deadline=$((SECONDS + TEST_TIMEOUT))
    until "$@"; do
        if (( SECONDS >= deadline )); then
            echo "Timed out waiting for: $*" >&2
            return 1
        fi
        sleep 0.05
    done
}

race_initialized() {
    local mask
    kill -0 "$RACE_PID" 2>/dev/null || return 1
    [[ -e /dev/shm/sem.race-"$RACE_PID"-car-$((CAR_COUNT - 1)) ]] || return 1
    # SIGINT must already have its handler installed before tests send it.
    mask=$(awk '$1 == "SigCgt:" {print $2}' "/proc/$RACE_PID/status" 2>/dev/null)
    [[ -n "$mask" ]] && (( (16#$mask & 2) != 0 ))
}

car_waiting_for_start() {
    local channel
    kill -0 "$1" 2>/dev/null || return 1
    channel=$(cat "/proc/$1/wchan" 2>/dev/null)
    [[ "$channel" == *futex* ]]
}

race_rendered() {
    grep -qF 'TestDriver0' "$TEST_DIR/race.stdout"
}

race_started() {
    grep -qF $'\033[32m▓' "$TEST_DIR/race.stdout"
}

wait_success() {
    local pid=$1
    local deadline=$((SECONDS + TEST_TIMEOUT))
    while kill -0 "$pid" 2>/dev/null; do
        if (( SECONDS >= deadline )); then
            echo "Process $pid did not finish within $TEST_TIMEOUT seconds" >&2
            return 1
        fi
        sleep 0.05
    done
    local rc=0
    wait "$pid" || rc=$?
    # This child is reaped; never signal its PID during cleanup.
    local remaining=() active
    for active in "${ACTIVE_PIDS[@]}"; do
        [[ "$active" == "$pid" ]] || remaining+=("$active")
    done
    ACTIVE_PIDS=("${remaining[@]}")
    if (( rc != 0 )); then
        echo "Process $pid exited with status $rc" >&2
        return 1
    fi
}

start_race() {
    CAR_COUNT=$1
    local laps=$2 delay=$3
    shift 3
    REPORT="$TEST_DIR/report.log"
    CAR_PIDS=()
    "$RACE" --cars "$CAR_COUNT" --map "$CIRCLE_MAP" --laps "$laps" \
        --draw-delay 10 --car-delay "$delay" --output "$REPORT" "$@" \
        >"$TEST_DIR/race.stdout" 2>"$TEST_DIR/race.stderr" &
    RACE_PID=$!
    ACTIVE_PIDS+=("$RACE_PID")
    wait_until race_initialized
}

start_car() {
    local id=$1
    shift
    "$CAR" --id "$id" --driver "TestDriver$id" --team "Test Team" \
        --color 33 --race-pid "$RACE_PID" "$@" \
        >"$TEST_DIR/car-$id.stdout" 2>"$TEST_DIR/car-$id.stderr" &
    CAR_PIDS+=("$!")
    ACTIVE_PIDS+=("$!")
}

start_all_cars() {
    local id
    for ((id = 0; id < CAR_COUNT; ++id)); do
        start_car "$id"
    done
}

finish_race() {
    wait_success "$RACE_PID" || return 1
    local pid
    for pid in "${CAR_PIDS[@]}"; do
        wait_success "$pid" || return 1
    done
    [[ -s "$REPORT" ]] || { echo "Race report is missing" >&2; return 1; }
    local leftovers
    leftovers=$(ipc_files)
    if [[ -n "$leftovers" ]]; then
        printf 'IPC resources were not removed:\n%s\n' "$leftovers" >&2
        return 1
    fi
    if [[ -s "$TEST_DIR/race.stderr" ]]; then
        cat "$TEST_DIR/race.stderr" >&2
        return 1
    fi
    for ((id = 0; id < CAR_COUNT; ++id)); do
        [[ ! -s "$TEST_DIR/car-$id.stderr" ]] || {
            cat "$TEST_DIR/car-$id.stderr" >&2
            return 1
        }
    done
}

expect_command() {
    local expected=$1 text=$2
    shift 2
    local rc=0
    timeout --kill-after=1 5 "$@" >"$TEST_DIR/command.stdout" 2>"$TEST_DIR/command.stderr" || rc=$?
    if (( rc >= 124 )); then
        echo "Command timed out or terminated abnormally: $rc" >&2
        return 1
    fi
    if [[ "$expected" == success ]]; then
        (( rc == 0 )) || return 1
    else
        (( rc != 0 )) || return 1
    fi
    grep -qF -- "$text" "$TEST_DIR/command.stdout" "$TEST_DIR/command.stderr"
}

normal_race() {
    start_race "$1" "$2" "$3" "${@:4}" || return 1
    start_all_cars
    finish_race || return 1
    grep -q 'Status *: Completed successfully' "$REPORT" &&
        grep -q 'Winner *: TestDriver' "$REPORT" &&
        grep -q "Participants *: $CAR_COUNT$" "$REPORT"
}

interrupt_race() {
    local stage=$1
    start_race 2 100000 20:30 || return 1
    if [[ "$stage" == partial ]]; then
        start_car 0
        wait_until car_waiting_for_start "${CAR_PIDS[0]}" || return 1
    elif [[ "$stage" != empty ]]; then
        start_all_cars
        if [[ "$stage" == countdown ]]; then
            wait_until race_rendered || return 1
        else
            wait_until race_started || return 1
        fi
    fi
    kill -INT "$RACE_PID" || return 1
    finish_race || return 1
    grep -q 'Status *: Interrupted by user' "$REPORT" &&
        grep -q 'Winner *: None' "$REPORT"
}

retire_car() {
    local mode=$1
    start_race 2 100000 20:30 || return 1
    start_all_cars
    wait_until race_started || return 1
    kill -INT "${CAR_PIDS[0]}" || return 1
    if [[ "$mode" == all ]]; then
        kill -INT "${CAR_PIDS[1]}" || return 1
    else
        wait_success "${CAR_PIDS[0]}" || return 1
        # Already reaped: finish_race should only wait for the remaining child.
        CAR_PIDS=("${CAR_PIDS[1]}")
        kill -INT "$RACE_PID" || return 1
    fi
    finish_race || return 1
    grep -q 'OUT' "$REPORT" || return 1
    if [[ "$mode" == all ]]; then
        grep -q 'Status *: All cars are out' "$REPORT" &&
            grep -q 'Winner *: None' "$REPORT"
    else
        grep -q 'Status *: Interrupted by user' "$REPORT"
    fi
}

retire_before_start() {
    start_race 2 1 1:3 || return 1
    start_car 0
    wait_until car_waiting_for_start "${CAR_PIDS[0]}" || return 1
    kill -INT "${CAR_PIDS[0]}" || return 1
    wait_success "${CAR_PIDS[0]}" || return 1
    CAR_PIDS=()
    start_car 1
    finish_race || return 1
    grep -q 'Winner *: TestDriver1' "$REPORT" &&
        grep -q 'OUT' "$REPORT"
}

duplicate_id() {
    start_race 2 1 1:3 || return 1
    start_car 0
    wait_until car_waiting_for_start "${CAR_PIDS[0]}" || return 1
    timeout --kill-after=1 5 "$CAR" -i 0 -n Duplicate -t Test -c 31 -r "$RACE_PID" \
        >"$TEST_DIR/duplicate.stdout" 2>"$TEST_DIR/duplicate.stderr"
    local rc=$?
    (( rc == 1 )) || return 1
    grep -q 'Car id is already taken' "$TEST_DIR/duplicate.stderr" || return 1
    start_car 1
    finish_race
}

run_test() {
    local name=$1
    shift
    TOTAL=$((TOTAL + 1))
    TEST_DIR="$LOG_DIR/test-$TOTAL"
    mkdir -p "$TEST_DIR"
    printf '[TEST %02d] %s... ' "$TOTAL" "$name"
    if "$@" >"$TEST_DIR/check.stdout" 2>"$TEST_DIR/check.stderr"; then
        echo PASSED
        PASSED=$((PASSED + 1))
    else
        echo FAILED
        FAILED=$((FAILED + 1))
        [[ ! -s "$TEST_DIR/check.stderr" ]] || cat "$TEST_DIR/check.stderr"
        echo "  Details: $TEST_DIR"
    fi
    cleanup
}

run_test 'race help' expect_command success 'Usage:' "$RACE" --help
run_test 'car help' expect_command success 'Usage:' "$CAR" --help
run_test 'Required car count' expect_command failure 'Specify the number of cars' "$RACE"
run_test 'Zero car count' expect_command failure 'Cars number must' "$RACE" -c 0
run_test 'Car count exceeds limit' expect_command failure 'Cars number must' "$RACE" -c 100000
run_test 'Negative lap count' expect_command failure 'Max laps must' "$RACE" -c 2 -l -3
run_test 'Zero draw delay' expect_command failure 'Draw delay must' "$RACE" -c 2 -d 0
run_test 'Invalid car delay range' expect_command failure 'Car min delay' "$RACE" -c 2 --car-delay 20:10
run_test 'Missing map file' expect_command failure 'Open file error' "$RACE" -c 2 -m "$LOG_DIR/missing-map"
run_test 'Missing car options' expect_command failure 'Specify --id' "$CAR"
run_test 'Negative car ID' expect_command failure 'Car ID must' "$CAR" --id -1
run_test 'Invalid ANSI color' expect_command failure 'Color must' "$CAR" --color 99
run_test 'Invalid race PID' expect_command failure 'Race PID must' "$CAR" --race-pid 0
run_test 'Driver name exceeds limit' expect_command failure 'Driver name must' "$CAR" --driver ABCDEFGHIJKLMNOPQRSTUVWXYZ
run_test 'Team name exceeds limit' expect_command failure 'Team name must' "$CAR" --team ABCDEFGHIJKLMNOPQRSTUVWXYZ
run_test 'Two independent cars finish' normal_race 2 1 1:3
run_test 'Four cars, three laps, zero delay' normal_race 4 3 0:0
run_test 'Map-only display' normal_race 2 1 1:3 --map-only
run_test 'SIGINT before any car connects' interrupt_race empty
run_test 'SIGINT with one connected car' interrupt_race partial
run_test 'SIGINT during countdown' interrupt_race countdown
run_test 'SIGINT during the race' interrupt_race running
run_test 'SIGINT retires one car' retire_car one
run_test 'All cars retire without a winner' retire_car all
run_test 'A car retires before the second car connects' retire_before_start
run_test 'Duplicate car ID is rejected' duplicate_id

printf '\nTotal: %d, passed: %d, failed: %d\nLogs: %s\n' "$TOTAL" "$PASSED" "$FAILED" "$LOG_DIR"
(( FAILED == 0 ))
