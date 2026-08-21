#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Script to non-interactively edit/update the Gerrit CL description for the current branch.
# It enforces commit description line-length formatting (max 78 chars) and code formatting.

if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
  echo "Usage: edit_cl_description.sh <\"New description string\" | -f <file> | + | ->"
  echo "Updates the Gerrit CL description for the current branch non-interactively after checking formatting."
  exit 0
fi

DESC=""

if [ "$1" = "-f" ] || [ "$1" = "--file" ]; then
  if [ -z "$2" ] || [ ! -f "$2" ]; then
    echo "Error: File '$2' not found."
    exit 1
  fi
  DESC=$(cat "$2")
elif [ "$1" = "-n" ] || [ "$1" = "-m" ] || [ "$1" = "--desc" ] || [ "$1" = "--message" ] || [ "$1" = "--new-description" ]; then
  DESC="$2"
elif [[ "$1" == -n=* ]] || [[ "$1" == -m=* ]] || [[ "$1" == --desc=* ]] || [[ "$1" == --message=* ]] || [[ "$1" == --new-description=* ]]; then
  DESC="${1#*=}"
elif [ "$1" = "+" ]; then
  DESC=$(git log -1 --pretty=%B 2>/dev/null)
elif [ "$1" = "-" ]; then
  DESC=$(cat)
elif [ -n "$1" ]; then
  DESC="$*"
elif [ ! -t 0 ]; then
  DESC=$(cat)
else
  echo "Error: No new description provided."
  echo "Usage: edit_cl_description.sh <\"New description string\" | -f <file> | + | ->"
  exit 1
fi

if [ -z "$DESC" ]; then
  echo "Error: The provided CL description is empty."
  exit 1
fi

# Guardrail: Early authentication check
if command -v gcertstatus >/dev/null 2>&1; then
  if ! gcertstatus -nocheck_ssh -quiet 2>/dev/null; then
    echo "Error: Authentication expired or missing. Please run 'gcert' before updating description."
    exit 1
  fi
fi

# Guardrail: Verify git cl format changes nothing
git cl format > /dev/null 2>&1
if ! git diff --quiet HEAD; then
  echo "Error: git cl format resulted in code formatting changes. Please review and commit code formatting changes before updating the CL description."
  exit 1
fi

echo "$DESC" | "$SCRIPT_DIR/validate_cl_description.py" || exit 1

# Guardrail: Check if branch is associated with an issue
ISSUE_OUTPUT=$(git cl issue 2>&1)
if [[ "$ISSUE_OUTPUT" == *"None"* ]]; then
  echo "Error: git cl issue reports no issue associated with this branch. Cannot update remote description."
  exit 1
fi

echo "Updating CL description on Gerrit..."
SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl desc -f -n "$DESC"
STATUS=$?

if [ $STATUS -ne 0 ]; then
  echo "Error updating CL description."
  exit $STATUS
fi

echo "CL description updated successfully."
