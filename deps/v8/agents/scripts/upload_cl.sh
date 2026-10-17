#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MODE="$1"
CHECK_MODE="$2"

if [ -z "$MODE" ] || [ -z "$CHECK_MODE" ]; then
  echo "Usage: upload_cl.sh <new|cur> <check|nocheck> [patchset_message] [--wip|--ready] [additional flags...]"
  exit 1
fi

if [ "$MODE" != "new" ] && [ "$MODE" != "cur" ]; then
  echo "Error: First argument must be 'new' or 'cur'."
  echo "Usage: upload_cl.sh <new|cur> <check|nocheck> [patchset_message] [--wip|--ready] [additional flags...]"
  exit 1
fi

if [ "$CHECK_MODE" != "check" ] && [ "$CHECK_MODE" != "nocheck" ]; then
  echo "Error: Second argument must be 'check' or 'nocheck'."
  echo "Usage: upload_cl.sh <new|cur> <check|nocheck> [patchset_message] [--wip|--ready] [additional flags...]"
  exit 1
fi

# Shift positional parameters so "$@" contains any additional flags passed to upload_cl.sh
if [ $# -ge 3 ] && [[ "$3" != -* ]]; then
  PATCH_MSG="${3:-Update patchset}"
  shift 3
else
  PATCH_MSG="Update patchset"
  if [ $# -ge 2 ]; then
    shift 2
  else
    shift $#
  fi
fi

# Guardrail: Check commit description line lengths (max 78 chars, ignoring lines with URLs)
DESC_TO_CHECK=""
CHECK_DESC=false
HAS_CUSTOM_DESC=false
READY_FOR_REVIEW=false
EXPLICIT_WIP=false
EXPLICIT_READY=false
HAS_DRY_RUN_OR_CQ=false
HAS_SEND_MAIL_OR_CQ=false
UPLOAD_ARGS=()

prev_arg=""
for arg in "$@"; do
  if [ "$arg" = "--ready" ]; then
    READY_FOR_REVIEW=true
    EXPLICIT_READY=true
    prev_arg="$arg"
    continue
  elif [ "$arg" = "--wip" ]; then
    READY_FOR_REVIEW=false
    EXPLICIT_WIP=true
    prev_arg="$arg"
    continue
  fi

  if [ "$arg" = "-s" ] || [ "$arg" = "--send-mail" ] || [ "$arg" = "--send-email" ]; then
    HAS_SEND_MAIL_OR_CQ=true
    # Do not forward --send-mail to git cl upload, as git_cl.py translates it
    # into %ready on git push, which would prematurely exit WIP before the CQ
    # dry-run passes.
    prev_arg="$arg"
    continue
  fi

  UPLOAD_ARGS+=("$arg")

  if [ "$arg" = "-d" ] || [ "$arg" = "--dry-run" ] || [ "$arg" = "--cq-dry-run" ] || [ "$arg" = "-c" ] || [ "$arg" = "--use-commit-queue" ]; then
    HAS_DRY_RUN_OR_CQ=true
  fi
  if [ "$arg" = "-c" ] || [ "$arg" = "--use-commit-queue" ]; then
    HAS_SEND_MAIL_OR_CQ=true
  fi

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

if [ "$EXPLICIT_WIP" = true ] && [ "$EXPLICIT_READY" = true ]; then
  echo "Error: Cannot specify both '--wip' and '--ready'."
  exit 1
fi

if [ "$MODE" = "new" ] && [ "$READY_FOR_REVIEW" = true ]; then
  echo "Error: Initial uploads ('new') must be uploaded as WIP first. Use 'cur' with '--ready' once the CL is ready for review."
  exit 1
fi

if [ "$READY_FOR_REVIEW" = false ] && [ "$HAS_SEND_MAIL_OR_CQ" = true ]; then
  echo "Error: Cannot send mail or use commit queue on a WIP upload. Pass '--ready' when the CL is ready for review."
  exit 1
fi

if [ "$READY_FOR_REVIEW" = true ]; then
  # Keep WIP CLs in WIP during the dry-run; wait-dry-run will switch the CL to
  # ready only after the CQ dry-run passes.
  if [ "$HAS_DRY_RUN_OR_CQ" = false ]; then
    UPLOAD_ARGS+=("--cq-dry-run")
  fi
else
  UPLOAD_ARGS+=("-o" "wip")
fi

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

GERRIT_WRAPPER="$SCRIPT_DIR/../shared/skills/gerrit-cli/scripts/gerrit_client_wrapper.py"
GERRIT_HOST="https://chromium-review.googlesource.com"

set_cl_wip() {
  local issue_num="$1"
  local msg="$2"
  local out
  if out=$(vpython3 "$GERRIT_WRAPPER" rawapi \
    --host "$GERRIT_HOST" \
    --path "/changes/${issue_num}/wip" \
    --method POST \
    --body "{\"message\": \"${msg//\"/\\\"}\"}" \
    --accept_status "200,204" 2>&1); then
    return 0
  fi
  if [[ "$out" == *"already work in progress"* ]]; then
    return 0
  fi
  echo "$out" >&2
  return 1
}

set_cl_ready() {
  local issue_num="$1"
  local msg="$2"
  local out
  if out=$(vpython3 "$GERRIT_WRAPPER" rawapi \
    --host "$GERRIT_HOST" \
    --path "/changes/${issue_num}/ready" \
    --method POST \
    --body "{\"message\": \"${msg//\"/\\\"}\"}" \
    --accept_status "200,204" 2>&1); then
    return 0
  fi
  if [[ "$out" == *"not work in progress"* ]]; then
    return 0
  fi
  echo "$out" >&2
  return 1
}

wait_for_cl_dry_run() {
  local issue_num="$1"
  local cl_url="$2"
  local poll_interval="${UPLOAD_CL_POLL_INTERVAL:-30}"
  local timeout="${UPLOAD_CL_TIMEOUT:-7200}"
  local start_time=$SECONDS

  while true; do
    local cl_status
    cl_status=$(git cl status --field=status 2>/dev/null)
    local try_out
    try_out=$(git cl try-results 2>&1)

    if [ "$cl_status" != "dry-run" ] && [ "$cl_status" != "commit" ] && \
       ! echo "$try_out" | grep -qE '^(Started:|Scheduled:)'; then
      if echo "$try_out" | grep -q '^Successes:' && \
         ! echo "$try_out" | grep -qE '^(Failures:|Infra Failures:|Canceled:|Other:|ERROR:)'; then
        echo "STATUS: passed ($cl_url)"
        echo "CQ dry-run passed. Switching CL $issue_num to ready for review..."
        if ! set_cl_ready "$issue_num" "CQ dry-run passed; marking ready for review."; then
          echo "Error: Failed to mark CL $issue_num ready for review." >&2
          return 1
        fi
        return 0
      else
        echo "STATUS: failed"
        echo "CQ dry-run failed. Switching CL $issue_num back to WIP until it is fixed..."
        echo "$try_out"
        set_cl_wip "$issue_num" "CQ dry-run failed; switching back to WIP until fixed."
        return 1
      fi
    fi

    if (( SECONDS - start_time >= timeout )); then
      echo "Error: Timed out after ${timeout}s waiting for CQ dry-run on $cl_url. Switching back to WIP..." >&2
      set_cl_wip "$issue_num" "CQ dry-run timed out; switching back to WIP."
      return 1
    fi

    sleep "$poll_interval"
  done
}

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
    UPLOAD_OUTPUT=$(SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl upload -t "Initial upload" "${UPLOAD_ARGS[@]}" 2>&1)
  else
    UPLOAD_OUTPUT=$(SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl upload --commit-description=+ -t "Initial upload" "${UPLOAD_ARGS[@]}" 2>&1)
  fi
  UPLOAD_STATUS=$?
else
  if [ "$HAS_ISSUE" = false ]; then
    echo "Error: Mode 'cur' specified, but git cl issue reports no issue associated with this branch. Link with the existing issue using \`git cl issue <issue-id>\`."
    exit 1
  fi
  UPLOAD_OUTPUT=$(SKIP_GCE_AUTH_FOR_GIT=1 EDITOR=cat git cl upload -t "$PATCH_MSG" "${UPLOAD_ARGS[@]}" 2>&1)
  UPLOAD_STATUS=$?

  # If the latest commit was already uploaded and only WIP/ready state or dry-run
  # is changing, git push may report "no new changes".
  if [ $UPLOAD_STATUS -ne 0 ] && [[ "$UPLOAD_OUTPUT" == *"no new changes"* ]]; then
    ISSUE_NUM=$(echo "$ISSUE_OUTPUT" | grep -oE 'Issue number: [0-9]+' | grep -oE '[0-9]+' | tail -n 1)
    if [ -n "$ISSUE_NUM" ]; then
      EXISTING_CL_URL="https://chromium-review.googlesource.com/c/v8/v8/+/$ISSUE_NUM"
      if [ "$READY_FOR_REVIEW" = true ]; then
        git cl set-commit -d
        UPLOAD_STATUS=$?
      else
        set_cl_wip "$ISSUE_NUM" "$PATCH_MSG"
        UPLOAD_STATUS=$?
      fi
      if [ $UPLOAD_STATUS -eq 0 ]; then
        UPLOAD_OUTPUT="$EXISTING_CL_URL"
      fi
    fi
  fi
fi

if [ $UPLOAD_STATUS -ne 0 ]; then
  echo "Error during git cl upload:"
  echo "$UPLOAD_OUTPUT"
  exit $UPLOAD_STATUS
fi

CL_URL=$(echo "$UPLOAD_OUTPUT" | grep -o 'https://chromium-review.googlesource.com/c/v8/v8/+/[0-9]*' | tail -n 1)
if [ -z "$CL_URL" ]; then
  ISSUE_NUM=$(echo "$ISSUE_OUTPUT" | grep -oE 'Issue number: [0-9]+' | grep -oE '[0-9]+' | tail -n 1)
  if [ -n "$ISSUE_NUM" ]; then
    CL_URL="https://chromium-review.googlesource.com/c/v8/v8/+/$ISSUE_NUM"
  fi
fi

if [ -n "$CL_URL" ]; then
  echo "$CL_URL"
else
  echo "$UPLOAD_OUTPUT"
fi

if [ "$READY_FOR_REVIEW" = true ] && [ -n "$CL_URL" ]; then
  CL_ISSUE_NUM=$(echo "$CL_URL" | grep -oE '[0-9]+$' | tail -n 1)
  echo "Waiting for CQ dry-run on $CL_URL (will switch to ready if it passes, or WIP if it fails)..."
  wait_for_cl_dry_run "$CL_ISSUE_NUM" "$CL_URL"
  DRY_RUN_STATUS=$?
  if [ $DRY_RUN_STATUS -ne 0 ]; then
    exit $DRY_RUN_STATUS
  fi
fi
