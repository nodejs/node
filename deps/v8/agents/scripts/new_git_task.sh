#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
TASK_ID=$1
if [ -z "$TASK_ID" ]; then
  echo "Usage: new_git_task.sh <task_id>"
  exit 1
fi

BRANCH_NAME="task-$TASK_ID"
WORKTREE_PATH="../$BRANCH_NAME"

echo "Creating worktree at $WORKTREE_PATH..."
git worktree add $WORKTREE_PATH main
cd $WORKTREE_PATH
git checkout -b $BRANCH_NAME

echo "Task environment isolated at $WORKTREE_PATH"
