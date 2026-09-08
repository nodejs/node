#!/bin/bash
# A utility to run a command multiple times and report the failure count.
# Usage: ./repro_check.sh "command to run" [count]

COMMAND=$1
COUNT=${2:-10}
FAILURES=0

if [ -z "$COMMAND" ]; then
  echo "Usage: $0 \"command\" [count]"
  exit 2
fi

echo "Testing reproduction $COUNT times..."
for i in $(seq 1 $COUNT); do
  # Run the command and capture exit code.
  # We assume a non-zero exit code means reproduction (crash or assertion fail).
  $COMMAND > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    FAILURES=$((FAILURES + 1))
    echo -n "F"
  else
    echo -n "."
  fi
done

echo ""
echo "Failures: $FAILURES / $COUNT"

# Exit with 0 if it reproduced at least once, 1 otherwise.
if [ $FAILURES -gt 0 ]; then
  exit 0
else
  exit 1
fi
