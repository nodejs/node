#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Helper to remove worktrees and clean up after a task.

TASK_ID=$1
if [ -z "$TASK_ID" ]; then
  echo "Usage: cleanup_worktree.sh <task_id>"
  exit 1
fi

TOPLEVEL=$(git rev-parse --show-toplevel 2>/dev/null)
COMMON_DIR=$(git rev-parse --git-common-dir 2>/dev/null)
if [ -z "$COMMON_DIR" ] || [ -z "$TOPLEVEL" ]; then
  echo "Error: Not in a git repository."
  exit 1
fi
V8_ROOT="$(cd "$COMMON_DIR/.." && pwd)"
WS_ROOT="$(dirname "$TOPLEVEL")"
RIFT=$(PATH="$PATH:$HOME/bin" command -v rift 2>/dev/null)

# Inside an existing Rift workspace: close the child subagent workspace.
if [ -n "$RIFT" ] && [ -f "$WS_ROOT/.rift_prime_directive.json" ]; then
  CURRENT_WS="$(basename "$WS_ROOT")"

  # Mirror create_worktree.sh: a bare TASK_ID names the subagent workspace only
  # when it already carries the prefix, otherwise derive it.
  if [[ "$TASK_ID" == "${CURRENT_WS}_subagent_"* ]]; then
    SUBAGENT_WS="$TASK_ID"
  else
    SUBAGENT_WS="${CURRENT_WS}_subagent_${TASK_ID}"
  fi

  if ! "$RIFT" -y subagent close "$TASK_ID" > /dev/null 2>&1; then
    "$RIFT" close -y "$SUBAGENT_WS" > /dev/null 2>&1
  fi
  exit 0
fi

BRANCH_NAME="task-$TASK_ID"

# Outside a Rift workspace: close the top-level Rift workspace if it exists.
if [ -n "$RIFT" ] && "$RIFT" path "$BRANCH_NAME" >/dev/null 2>&1; then
  "$RIFT" close -y "$BRANCH_NAME" > /dev/null 2>&1
  exit 0
fi

WORKTREE_PATH="$V8_ROOT/worktrees/$BRANCH_NAME"

git worktree remove --force "$WORKTREE_PATH" > /dev/null 2>&1
git branch -D "$BRANCH_NAME" > /dev/null 2>&1
