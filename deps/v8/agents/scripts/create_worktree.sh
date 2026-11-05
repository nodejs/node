#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
TASK_ID=$1
BASE_BRANCH="${2:-origin/main}"
if [ -z "$TASK_ID" ]; then
  echo "Usage: create_worktree.sh <task_id> [base_branch]"
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

# Inside an existing Rift workspace: fork a child subagent workspace.
if [ -n "$RIFT" ] && [ -f "$WS_ROOT/.rift_prime_directive.json" ]; then
  CURRENT_WS="$(basename "$WS_ROOT")"
  RIFT_PARENT_DIR="$(dirname "$WS_ROOT")"
  REPO_DIR_NAME="$(basename "$TOPLEVEL")"

  if [[ "$TASK_ID" == "${CURRENT_WS}_subagent_"* ]]; then
    SUBAGENT_WS="$TASK_ID"
  else
    SUBAGENT_WS="${CURRENT_WS}_subagent_${TASK_ID}"
  fi
  WORKTREE_PATH="$RIFT_PARENT_DIR/$SUBAGENT_WS/$REPO_DIR_NAME"

  if [ ! -d "$WORKTREE_PATH" ]; then
    # Attempt Rift's subagent fork command as documented in the rift skill.
    # Fallback to standard 'rift fork' if the subagent subcommand fails or is unsupported.
    if ! "$RIFT" -y subagent fork "$TASK_ID" > /dev/null 2>&1; then
      "$RIFT" fork -y "$SUBAGENT_WS" --no-build > /dev/null 2>&1
    fi
  fi

  if [ ! -d "$WORKTREE_PATH" ]; then
    echo "Error: Failed to create rift workspace $SUBAGENT_WS" >&2
    exit 1
  fi

  echo "$WORKTREE_PATH"
  exit 0
fi

BRANCH_NAME="task-$TASK_ID"

# Outside a Rift workspace with Rift configured for V8: open a top-level Rift workspace.
if [ -n "$RIFT" ] && \
   [ "$("$RIFT" config get workspace_root 2>/dev/null)" = "workspace_root = v8" ]; then
  if "$RIFT" path "$BRANCH_NAME" >/dev/null 2>&1 || \
     "$RIFT" open -y "$BRANCH_NAME" --upstream "$BASE_BRANCH" --no-sync --no-build > /dev/null 2>&1; then
    echo "$("$RIFT" path "$BRANCH_NAME")/v8"
    exit 0
  fi
fi

WORKTREE_PATH="$V8_ROOT/worktrees/$BRANCH_NAME"

mkdir -p "$V8_ROOT/worktrees"

if ! git worktree add --track -b "$BRANCH_NAME" "$WORKTREE_PATH" "$BASE_BRANCH" 2>/dev/null && \
   ! git worktree add -b "$BRANCH_NAME" "$WORKTREE_PATH" "$BASE_BRANCH"; then
  echo "Error: Failed to create git worktree at $WORKTREE_PATH" >&2
  exit 1
fi

if ! python3 "$V8_ROOT/tools/dev/setup_worktree_build.py" "$V8_ROOT" "$WORKTREE_PATH"; then
  echo "Error: Failed to set up dependencies for $WORKTREE_PATH" >&2
  exit 1
fi

echo "$WORKTREE_PATH"
