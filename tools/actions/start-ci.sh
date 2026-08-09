#!/bin/sh

set -xe

REQUEST_CI_LABEL="request-ci"
REQUEST_CI_FAILED_LABEL="request-ci-failed"
cqurl="${GITHUB_SERVER_URL?:}/${GITHUB_REPOSITORY?:}/actions/runs/${GITHUB_RUN_ID?:}"

for pr in "$@"; do
  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --remove-label "$REQUEST_CI_LABEL"

  ci_started=yes
  rm -f output;
  ncu-ci run --check-for-duplicates "$pr" >output 2>&1 || ci_started=no
  cat output

  if [ "$ci_started" = "no" ]; then
    # Do we need to reset?
    gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --add-label "$REQUEST_CI_FAILED_LABEL"

    body="<details><summary>Failed to start CI</summary><pre>$(cat output)</pre><a href='$cqurl'>$cqurl</a></details>"
    echo "$body"

    gh -R "$GITHUB_REPOSITORY" pr comment "$pr" --body "$body"

    rm output
  fi
done;
