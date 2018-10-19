#!/usr/bin/env bash

# Shell script to lint commit messages
#
# Depends on git, node, npm and npx being in $PATH.
#
# This script must be be in the tools directory when it runs because it uses
# $BASH_SOURCE[0] to determine directories to work in.
#
# This script attempts to guess the target upstream branch. To manually specify:
# e.g. to target `canary-base`:
# TARGET_BRANCH=canary-base bash lint-commit-message.sh

ROOT_DIR="$( dirname "${BASH_SOURCE[0]}" )/.."

# Work out what branch is being targetted
MAJOR_VERSION=$( grep '#define NODE_MAJOR_VERSION [0-9][0-9]*' "${ROOT_DIR}/src/node_version.h" | awk '{print $(NF)}' )
MINOR_VERSION=$( grep '#define NODE_MINOR_VERSION [0-9][0-9]*' "${ROOT_DIR}/src/node_version.h" | awk '{print $(NF)}' )
PATCH_VERSION=$( grep '#define NODE_PATCH_VERSION [0-9][0-9]*' "${ROOT_DIR}/src/node_version.h" | awk '{print $(NF)}' )
# If the version is not in the CHANGELOG assume the target branch is master
if grep -q "${MAJOR_VERSION}\.${MINOR_VERSION}\.${PATCH_VERSION}" "${ROOT_DIR}/CHANGELOG.md"; then
  TARGET_BRANCH=${TARGET_BRANCH:-"v${MAJOR_VERSION}.x-staging"}
else
  TARGET_BRANCH=${TARGET_BRANCH:-"master"}
fi
echo "Target branch is ${TARGET_BRANCH}"

# Make sure we're up to date
UPSTREAM=$( git remote -v | grep "github\.com.*nodejs/node.* (fetch)" | awk '{print $(1)}' )
echo "Fetching upstream remote ${UPSTREAM}"
git fetch "${UPSTREAM}" "${TARGET_BRANCH}":"${TARGET_BRANCH}"
COMMON_ANCESTOR=$( git merge-base "${UPSTREAM}/$TARGET_BRANCH" HEAD )
FIRST_COMMIT=$( git rev-list --no-merges ${COMMON_ANCESTOR}..HEAD | tail -1 )

if [ -n "${FIRST_COMMIT}" ]; then
  echo "Linting the first commit message according to the guidelines at https://goo.gl/p2fr5Q"
  echo "$( git log -1 ${FIRST_COMMIT} )"
  echo "${FIRST_COMMIT}" | xargs npx -q core-validate-commit --no-validate-metadata
else
  echo "No commits found to lint"
fi
