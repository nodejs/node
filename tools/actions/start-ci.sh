#!/bin/sh

set -xe

REQUEST_CI_LABEL="request-ci"
REQUEST_CI_FAILED_LABEL="request-ci-failed"

for pr in "$@"; do
  gh pr edit "$pr" --remove-label "$REQUEST_CI_LABEL"

  ci_started=yes
  rm -f output;
  ncu-ci run --certify-safe "$pr" >output 2>&1 || ci_started=no
  cat output

  if [ "$ci_started" = "no" ]; then
    # Do we need to reset?
    gh pr edit "$pr" --add-label "$REQUEST_CI_FAILED_LABEL"

    # shellcheck disable=SC2154
    cqurl="${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
    body="<details><summary>Failed to start CI</summary><pre>$(cat output)</pre><a href='$cqurl'>$cqurl</a></details>"
    echo "$body"

    gh pr comment "$pr" --body "$body"

    rm output
  fi
done;
