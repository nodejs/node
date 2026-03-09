#!/usr/bin/env bash
#
# Copyright 2025 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

set -euo pipefail

# This allows us to leave XODO comments in this file and have them still be
# picked up by this script without having the script itself trigger false
# positives. The alternative would be to exclude this script entirely, which
# would mean that we couldn't use XODO comments in this script.
KEYWORD=$(echo XODO | sed -e 's/X/T/')

# Make sure `rg` is installed (if this fails, `set -e` above will cause the
# script to exit).
rg --version >/dev/null

# -H: Print filename (default for multiple files/recursive)
# -n: Print line number
# -w: Match whole words
# Match TODO, TODO-check-disable, and TODO-check-enable
PATTERN="$KEYWORD|$KEYWORD-check-disable|$KEYWORD-check-enable"
output=$(rg -H -n -w "$PATTERN" "$@" || true)

commit_output=$(git log -1 --pretty=%B 2>/dev/null | rg -n -w "$PATTERN" || true)
if [ -n "$commit_output" ]; then
  commit_output=$(echo "$commit_output" | sed "s/^/COMMIT_MESSAGE:/")
  output="$commit_output${output:+$'\n'$output}"
fi

if [ -z "$output" ]; then
  exit 0
fi

# Track the disabled state for each file. Since we process lines in order for
# each file, we can maintain state. However, rg output might interleave files if
# not sorted, but usually it's grouped. To be safe, we sort the output by
# filename and line number.
sorted_output=$(echo "$output" | sort -t: -k1,1 -k2,2n)

exit_code=0
current_file=""
disabled=0

while IFS= read -r line; do
  if [[ "$line" =~ ^(.*):([0-9]+):(.*)$ ]]; then
    file="${BASH_REMATCH[1]}"
    content="${BASH_REMATCH[3]}"
  else
    echo "Error: could not parse rg output line: $line" >&2
    exit 1
  fi

  if [ "$file" != "$current_file" ]; then
    current_file="$file"
    disabled=0
  fi

  if [[ "$content" == *"$KEYWORD-check-disable"* ]]; then
    disabled=1
  elif [[ "$content" == *"$KEYWORD-check-enable"* ]]; then
    disabled=0
  elif [[ "$content" == *"$KEYWORD"* ]]; then
    if [ "$disabled" -eq 0 ]; then
      if [ "$exit_code" -eq 0 ]; then
        echo "Found $KEYWORD markers in the codebase." >&2
        echo "$KEYWORD is used for tasks that should be done before merging a PR; if you want to leave a message in the codebase, use FIXME." >&2
        echo "If you need to allow a $KEYWORD, wrap it in $KEYWORD-check-disable and $KEYWORD-check-enable." >&2
        echo "" >&2
        exit_code=1
      fi
      echo "$line" >&2
    fi
  fi
done <<< "$sorted_output"

exit $exit_code
