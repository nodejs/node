#!/bin/sh
set -e
# Shell script to update npm in the source tree to a specific version

BASE_DIR=$(cd "$(dirname "$0")/.." && pwd)

cd "$BASE_DIR"

TARGET_RELEASE=$1

PR_NUMBER=$2

if [ "$#" -ne 2 ]; then
    echo "Please provide target branch and PR number"
    exit 1
fi

git checkout "v$TARGET_RELEASE.x-staging"

git node backport "$PR_NUMBER" --to "$TARGET_RELEASE"
