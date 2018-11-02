#!/usr/bin/env bash

# Shell script to lint the message of the first commit in a pull request.
#
# Depends on curl, git, node, npm and npx being in $PATH.
#
# The pull request is either:
# 1) supplied as an argument to this shell script
# 2) derived from the HEAD commit via the GitHub API

GH_API_URL="https://api.github.com"
PR_ID=$1;
if [ -z "${PR_ID}" ]; then
  # Attempt to work out the PR number based on current HEAD
  if HEAD_COMMIT="$( git rev-parse HEAD )"; then
    if SEARCH_RESULTS="$( curl -s ${GH_API_URL}/search/issues?q=sha:${HEAD_COMMIT}+type:pr+repo:nodejs/node )"; then
      if FOUND_PR="$( node -p 'JSON.parse(process.argv[1]).items[0].number' "${SEARCH_RESULTS}" 2> /dev/null )"; then
        PR_ID=${FOUND_PR}
      fi
    fi
  fi
fi
if [ -z "${PR_ID}" ]; then
  echo "Unable to determine the pull request number to check. Please specify, "
  echo " e.g. $0 <PR_NUMBER>"
  exit 1
fi
# Retrieve the first commit of the pull request via GitHub API
# TODO: If we teach core-validate-commit to ignore "fixup!" and "squash!"
#       commits and lint messages for all commits in the pull request
#       we could simplify the following to:
#         npx -q core-validate-commit --no-validate-metadata ${GH_API_URL}/repos/nodejs/node/pulls/${PR_ID}/commits
if PR_COMMITS="$( curl -s ${GH_API_URL}/repos/nodejs/node/pulls/${PR_ID}/commits )"; then
  if FIRST_COMMIT="$( node -p 'JSON.parse(process.argv[1])[0].url' "${PR_COMMITS}" 2> /dev/null )"; then
    echo "Linting the first commit message for pull request ${PR_ID}"
    echo "according to the guidelines at https://goo.gl/p2fr5Q."
    # Print the commit message to make it more obvious what is being checked.
    echo "Commit message for ${FIRST_COMMIT##*/} is:"
    node -p 'JSON.parse(process.argv[1])[0].commit.message' "${PR_COMMITS}" 2> /dev/null
    npx -q core-validate-commit --no-validate-metadata "${FIRST_COMMIT}"
  else
    echo "Unable to determine the first commit for pull request ${PR_ID}."
    exit 1
  fi
fi
