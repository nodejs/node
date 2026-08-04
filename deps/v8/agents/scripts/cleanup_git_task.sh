#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Helper to remove worktrees and clean up after a task.

TASK_ID=$1
if [ -z "$TASK_ID" ]; then
  echo "Usage: cleanup_git_task.sh <task_id>"
  exit 1
fi

BRANCH_NAME="task-$TASK_ID"
WORKTREE_PATH="../$BRANCH_NAME"

echo "Removing worktree at $WORKTREE_PATH..."
git worktree remove $WORKTREE_PATH
git branch -d $BRANCH_NAME
echo "Cleanup complete."
