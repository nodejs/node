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

if command -v rift >/dev/null 2>&1 && [ -f "$WS_ROOT/.rift_prime_directive.json" ]; then
  CURRENT_WS="$(basename "$WS_ROOT")"
  RIFT_PARENT_DIR="$(dirname "$WS_ROOT")"

  if [[ "$TASK_ID" == "${CURRENT_WS}_subagent_"* ]] || [ -d "$RIFT_PARENT_DIR/$TASK_ID" ]; then
    SUBAGENT_WS="$TASK_ID"
  else
    SUBAGENT_WS="${CURRENT_WS}_subagent_${TASK_ID}"
  fi

  if ! rift -y subagent close "$TASK_ID" > /dev/null 2>&1; then
    rift close -y "$SUBAGENT_WS" > /dev/null 2>&1
  fi
  exit 0
fi

BRANCH_NAME="task-$TASK_ID"
WORKTREE_PATH="$V8_ROOT/worktrees/$BRANCH_NAME"

git worktree remove --force "$WORKTREE_PATH" > /dev/null 2>&1
git branch -D "$BRANCH_NAME" > /dev/null 2>&1
