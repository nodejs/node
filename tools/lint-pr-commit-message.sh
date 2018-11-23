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

PATCH=$( curl -sL https://github.com/nodejs/node/pull/${PR_ID}.patch | grep '^From ' )
if FIRST_COMMIT="$( echo "$PATCH" | awk '/^From [0-9a-f]{40} / { if (count++ == 0) print $2 }' )"; then
  MESSAGE=$( git show --quiet --format='format:%B' $FIRST_COMMIT )
  echo "
*** Linting the first commit message for pull request ${PR_ID}
*** according to the guidelines at https://goo.gl/p2fr5Q.
*** Commit message for $(echo $FIRST_COMMIT | cut -c 1-10) is:
${MESSAGE}
"
  npx -q core-validate-commit --no-validate-metadata "${FIRST_COMMIT}"
fi
