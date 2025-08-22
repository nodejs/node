#!/bin/bash

# Manual sync script for dharani-dharan-11023/node repository
# This script helps manually sync the fork with the upstream Node.js repository

set -e

echo "🔄 Manual sync with upstream Node.js repository"
echo "================================================="

# Add upstream remote if it doesn't exist
if ! git remote get-url upstream >/dev/null 2>&1; then
    echo "➕ Adding upstream remote..."
    git remote add upstream https://github.com/nodejs/node.git
else
    echo "✅ Upstream remote already exists"
fi

# Fetch latest changes
echo "📥 Fetching latest changes from upstream..."
git fetch upstream
git fetch origin

# Get current branch
CURRENT_BRANCH=$(git branch --show-current)
echo "📍 Current branch: $CURRENT_BRANCH"

# Check if main branch exists
if git show-ref --verify --quiet refs/heads/main; then
    echo "✅ Local main branch exists"
    git checkout main
elif git show-ref --verify --quiet refs/remotes/origin/main; then
    echo "🆕 Creating local main branch from origin/main"
    git checkout -b main origin/main
else
    echo "🆕 Creating new main branch from upstream/main"
    git checkout -b main upstream/main
fi

# Get commit info
UPSTREAM_SHA=$(git rev-parse upstream/main)
LOCAL_SHA=$(git rev-parse HEAD)

echo "📊 Commit comparison:"
echo "  Local main:    $LOCAL_SHA"
echo "  Upstream main: $UPSTREAM_SHA"

if [ "$UPSTREAM_SHA" = "$LOCAL_SHA" ]; then
    echo "✅ Repository is already up to date!"
    exit 0
fi

echo "🔄 Attempting to sync..."

# Try fast-forward merge
if git merge --ff-only upstream/main; then
    echo "✅ Successfully fast-forwarded to upstream!"
    echo "📤 Pushing changes to origin..."
    git push origin main
    echo "🎉 Sync completed successfully!"
else
    echo "⚠️  Fast-forward merge failed. Manual intervention needed."
    echo "🔧 You can try one of the following:"
    echo "   1. git merge upstream/main  (create merge commit)"
    echo "   2. git rebase upstream/main (rebase your changes)"
    echo "   3. git reset --hard upstream/main (discard local changes)"
    
    # Show what commits are different
    echo ""
    echo "📋 Commits that will be added from upstream:"
    git log --oneline $LOCAL_SHA..upstream/main | head -10
    
    echo ""
    echo "📋 Local commits not in upstream:"
    git log --oneline upstream/main..$LOCAL_SHA | head -10
fi

# Return to original branch if different
if [ "$CURRENT_BRANCH" != "main" ] && [ -n "$CURRENT_BRANCH" ]; then
    echo "🔄 Returning to original branch: $CURRENT_BRANCH"
    git checkout "$CURRENT_BRANCH"
fi