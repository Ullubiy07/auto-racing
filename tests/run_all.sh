#!/usr/bin/env bash
set -euo pipefail

script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

if [[ $# -gt 1 ]]; then
    printf 'Usage: %s [BUILD_DIR]\n' "$0" >&2
    exit 1
fi
if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    printf 'Usage: %s [BUILD_DIR]\n' "$0"
    printf 'Build the programs, then run judge and TCP tests.\n'
    exit 0
fi

for tool in cmake ctest python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required command not found: %s\n' "$tool" >&2
        exit 1
    fi
done

test_build_dir=${1:-"$project_dir/build"}
cmake -S "$project_dir" -B "$test_build_dir"
cmake --build "$test_build_dir" --target race car viewer control tests --parallel
ctest --test-dir "$test_build_dir" --output-on-failure

# Исполняемые файлы лежат в ../bin относительно каталога сборки.
test_bin_dir=$(CDPATH= cd -- "$test_build_dir/../bin" && pwd)
RACING_BIN_DIR="$test_bin_dir" python3 "$script_dir/test_tcp.py"

printf 'All tests passed.\n'
