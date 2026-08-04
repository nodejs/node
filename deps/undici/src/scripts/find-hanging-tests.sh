#!/usr/bin/env bash
set -euo pipefail

TIMEOUT_SECONDS=${TIMEOUT_SECONDS:-120}
TEST_GLOB=${TEST_GLOB:-"test/*.js"}
LOG_DIR=${LOG_DIR:-"/tmp/undici-test-hang"}

mkdir -p "$LOG_DIR"

failures=()
timeouts=()

while IFS= read -r test_file; do
  echo "==> ${test_file}"
  log_file="$LOG_DIR/$(basename "$test_file").log"
  if timeout "$TIMEOUT_SECONDS" npx borp -p "$test_file" >"$log_file" 2>&1; then
    echo "PASS ${test_file}"
  else
    exit_code=$?
    if [[ $exit_code -eq 124 || $exit_code -eq 137 ]]; then
      echo "TIMEOUT ${test_file}"
      timeouts+=("$test_file")
    else
      echo "FAIL ${test_file} (exit $exit_code)"
      failures+=("$test_file")
    fi
  fi
  echo
  sleep 0.2
done < <(ls $TEST_GLOB | sort)

echo "=== Summary ==="
if [[ ${#timeouts[@]} -gt 0 ]]; then
  echo "Timeouts:"
  printf '  - %s\n' "${timeouts[@]}"
else
  echo "Timeouts: none"
fi

if [[ ${#failures[@]} -gt 0 ]]; then
  echo "Failures:"
  printf '  - %s\n' "${failures[@]}"
else
  echo "Failures: none"
fi

echo "Logs: $LOG_DIR"
