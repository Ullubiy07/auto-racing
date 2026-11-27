#!/usr/bin/env bash
set -euo pipefail

project_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if [[ $# -gt 1 ]]; then
    printf 'Usage: %s [PORT]\n' "$0" >&2
    exit 1
fi
if [[ "${1:-}" == '--help' || "${1:-}" == '-h' ]]; then
    printf 'Usage: %s [PORT]\n' "$0"
    printf 'Run a three-lap race on twisty with 14 cars and a viewer (default port: 8080).\n'
    printf 'Press Ctrl+C to stop the demo.\n'
    exit 0
fi

for program in race car viewer; do
    if [[ ! -x "$project_dir/bin/$program" ]]; then
        printf 'Missing bin/%s. Build the project first:\n' "$program" >&2
        printf '  cmake -S "%s" -B "%s/build" -DCMAKE_BUILD_TYPE=Release\n' "$project_dir" "$project_dir" >&2
        printf '  cmake --build "%s/build"\n' "$project_dir" >&2
        exit 1
    fi
done

port=${1:-8080}
log_dir="$project_dir/logs/demo_$(date +%Y-%m-%d_%H-%M-%S)_$$"
mkdir -p -- "$log_dir"

server_pid=''
client_pids=()

cleanup() {
    trap - EXIT
    trap '' INT TERM

    if [[ -n "$server_pid" ]]; then
        kill -TERM "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi

    for pid in "${client_pids[@]}"; do
        kill -TERM "$pid" 2>/dev/null || true
    done
    for pid in "${client_pids[@]}"; do
        wait "$pid" 2>/dev/null || true
    done

    printf '\nLogs: %s\n' "$log_dir" >&2
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

"$project_dir/bin/race" --cars 14 --laps 3 --ip 127.0.0.1 --port "$port" \
    --map "$project_dir/data/twisty" --output "$log_dir/race.log" >"$log_dir/server.log" 2>&1 &
server_pid=$!

ready=false
for ((attempt = 0; attempt < 100; ++attempt)); do
    if grep -q 'Race server listening' "$log_dir/server.log"; then
        ready=true
        break
    fi
    if ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$log_dir/server.log" >&2
        exit 1
    fi
    sleep 0.05
done
if [[ "$ready" != true ]]; then
    printf 'Server did not start listening.\n' >&2
    cat "$log_dir/server.log" >&2
    exit 1
fi

start_team() {
    local team=$1
    local color=$2
    shift 2

    for driver in "$@"; do
        "$project_dir/bin/car" --driver "$driver" --team "$team" --color "$color" \
            --ip 127.0.0.1 --port "$port" >"$log_dir/$driver.log" 2>&1 &
        client_pids+=("$!")
    done
}

start_team 'Red Bull' 36 Max Liam
start_team 'Ferrari' 31 Charles Lewis
start_team 'Mercedes' 34 George Kimi
start_team 'McLaren' 33 Lando Oscar
start_team 'Aston Martin' 32 Fernando Lance
start_team 'Alpine' 35 Pierre Esteban
start_team 'Williams' 37 Alex Carlos

"$project_dir/bin/viewer" --ip 127.0.0.1 --port "$port" &
viewer_pid=$!
client_pids+=("$viewer_pid")
wait "$viewer_pid"
