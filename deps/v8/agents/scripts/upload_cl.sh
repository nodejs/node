#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MODE="$1"
CHECK_MODE="$2"
PATCH_MSG="${3:-Update patchset}"

if [ -z "$MODE" ] || [ -z "$CHECK_MODE" ]; then
  echo "Usage: upload_cl.sh <new|cur> <check|nocheck> [patchset_message]"
  exit 1
fi

if [ "$MODE" != "new" ] && [ "$MODE" != "cur" ]; then
  echo "Error: First argument must be 'new' or 'cur'."
  echo "Usage: upload_cl.sh <new|cur> <check|nocheck> [patchset_message]"
  exit 1
fi

if [ "$CHECK_MODE" != "check" ] && [ "$CHECK_MODE" != "nocheck" ]; then
  echo "Error: Second argument must be 'check' or 'nocheck'."
  echo "Usage: upload_cl.sh <new|cur> <check|nocheck> [patchset_message]"
  exit 1
fi

# Shift positional parameters so "$@" contains any additional flags passed to upload_cl.sh
if [ $# -ge 3 ]; then
  shift 3
else
  shift $#
fi

# Guardrail: Check commit description line lengths (max 78 chars, ignoring lines with URLs)
DESC_TO_CHECK=""
CHECK_DESC=false
HAS_CUSTOM_DESC=false

prev_arg=""
for arg in "$@"; do
  if [[ "$arg" == --commit-description=* ]] || [[ "$arg" == --message=* ]] || [[ "$arg" == -m=* ]]; then
    HAS_CUSTOM_DESC=true
    val="${arg#*=}"
    if [ "$val" != "+" ]; then
      DESC_TO_CHECK="$val"
      CHECK_DESC=true
    else
      DESC_TO_CHECK=$(git log -1 --pretty=%B 2>/dev/null)
      CHECK_DESC=true
    fi
  elif [ "$arg" == "-m+" ]; then
    HAS_CUSTOM_DESC=true
    DESC_TO_CHECK=$(git log -1 --pretty=%B 2>/dev/null)
    CHECK_DESC=true
  elif [ "$prev_arg" == "--commit-description" ] || [ "$prev_arg" == "--message" ] || [ "$prev_arg" == "-m" ]; then
    HAS_CUSTOM_DESC=true
    if [ "$arg" != "+" ]; then
      DESC_TO_CHECK="$arg"
      CHECK_DESC=true
    else
      DESC_TO_CHECK=$(git log -1 --pretty=%B 2>/dev/null)
      CHECK_DESC=true
    fi
  elif [ "$arg" == "--commit-description" ] || [ "$arg" == "--message" ] || [ "$arg" == "-m" ]; then
    HAS_CUSTOM_DESC=true
  fi
  prev_arg="$arg"
done

# If uploading a new CL and no explicit commit description flag was passed, check local commit message
if [ "$CHECK_DESC" = false ] && [ "$MODE" = "new" ]; then
  DESC_TO_CHECK=$(git log -1 --pretty=%B 2>/dev/null)
  CHECK_DESC=true
fi

if [ "$CHECK_DESC" = true ] && [ -n "$DESC_TO_CHECK" ]; then
  echo "$DESC_TO_CHECK" | "$SCRIPT_DIR/validate_cl_description.py" || exit 1
fi

# Guardrail: Early authentication check
if command -v gcertstatus >/dev/null 2>&1; then
  if ! gcertstatus -nocheck_ssh -quiet 2>/dev/null; then
    echo "Error: Authentication expired or missing. Please run 'gcert' before uploading."
    exit 1
  fi
fi

# Guardrail: Never upload directly from main branch
CUR_BRANCH=$(git branch --show-current)
if [ "$CUR_BRANCH" = "main" ] || [ "$CUR_BRANCH" = "master" ]; then
  echo "Error: Cannot upload directly from the main branch. Please switch to an isolated task branch."
  exit 1
fi

# Guardrail: Verify git diff is not empty before uploading
if git diff --quiet origin/main; then
  echo "Error: Branch has no changes relative to origin/main. Cannot upload an empty CL."
  exit 1
fi

# Guardrail: Verify no edited files are uncommitted
if ! git diff --quiet HEAD; then
  echo "Error: Uncommitted changes detected. Please commit or stash all tracked modified files before uploading."
  exit 1
fi

# Guardrail: Ensure git cl format changes nothing
git cl format > /dev/null 2>&1
if ! git diff --quiet HEAD; then
  echo "Error: git cl format resulted in formatting changes. Please review and commit formatting changes before uploading."
  exit 1
fi

# Guardrail: Ensure gm.py release checks pass if check requested
if [ "$CHECK_MODE" = "check" ]; then
  V8_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

  ARCH=$(uname -m)
  if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    V8_ARCH="arm64"
  elif [ "$ARCH" = "x86_64" ]; then
    V8_ARCH="x64"
  elif [ "$ARCH" = "i686" ] || [ "$ARCH" = "i386" ]; then
    V8_ARCH="ia32"
  else
    V8_ARCH="x64"
  fi

  echo "Running '$V8_ROOT/tools/dev/gm.py quiet ${V8_ARCH}.release mjsunit'. This might take a while."
  if ! "$V8_ROOT/tools/dev/gm.py" "quiet" "${V8_ARCH}.release" "mjsunit"; then
    echo "Error: Tests failed (gm.py ${V8_ARCH}.release.check). Ensure all release checks pass before uploading."
    exit 1
  fi
fi

ISSUE_OUTPUT=$(git cl issue 2>&1)
if [[ "$ISSUE_OUTPUT" == *"None"* ]]; then
  HAS_ISSUE=false
else
  HAS_ISSUE=true
fi

if [ "$MODE" = "new" ]; then
  if [ "$HAS_ISSUE" = true ]; then
    echo "Error: Mode 'new' specified, but git cl issue reports this branch is already associated with an issue."
    exit 1
  fi
  if [ "$HAS_CUSTOM_DESC" = true ]; then
    UPLOAD_OUTPUT=$(SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl upload -t "Initial upload" "$@" 2>&1)
  else
    UPLOAD_OUTPUT=$(SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl upload --commit-description=+ -t "Initial upload" "$@" 2>&1)
  fi
  UPLOAD_STATUS=$?
else
  if [ "$HAS_ISSUE" = false ]; then
    echo "Error: Mode 'cur' specified, but git cl issue reports no issue associated with this branch. Link with the existing issue using \`git cl issue <issue-id>\`."
    exit 1
  fi
  UPLOAD_OUTPUT=$(SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl upload -t "$PATCH_MSG" "$@" 2>&1)
  UPLOAD_STATUS=$?
fi

if [ $UPLOAD_STATUS -ne 0 ]; then
  echo "Error during git cl upload:"
  echo "$UPLOAD_OUTPUT"
  exit $UPLOAD_STATUS
fi

CL_URL=$(echo "$UPLOAD_OUTPUT" | grep -o 'https://chromium-review.googlesource.com/c/v8/v8/+/[0-9]*' | head -n 1)
if [ -n "$CL_URL" ]; then
  echo "$CL_URL"
else
  echo "$UPLOAD_OUTPUT"
fi
