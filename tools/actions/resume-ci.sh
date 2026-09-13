#!/bin/sh

set -xe

RESUME_CI_LABEL="resume-ci"
RESUME_CI_FAILED_LABEL="resume-ci-failed"
cqurl="${GITHUB_SERVER_URL:?}/${GITHUB_REPOSITORY:?}/actions/runs/${GITHUB_RUN_ID:?}"

for pr in "$@"; do
  start_requested=$(gh -R "$GITHUB_REPOSITORY" pr view "$pr" --json labels \
    --jq 'any(.labels[]; .name == "request-ci")')
  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --remove-label "$RESUME_CI_LABEL"

  ci_resumed=yes
  rm -f output;
  if [ "$start_requested" = "true" ]; then
    echo 'Refusing to resume CI while the request-ci label is present' >output
    ci_resumed=no
  else
    ncu-ci resume "$pr" >output 2>&1 || ci_resumed=no
  fi
  cat output

  if [ "$ci_resumed" = "no" ]; then
    gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --add-label "$RESUME_CI_FAILED_LABEL"

    body="<details><summary>Failed to resume CI</summary><pre>$(cat output)</pre><a href='$cqurl'>$cqurl</a></details>"
    echo "$body"

    gh -R "$GITHUB_REPOSITORY" pr comment "$pr" --body "$body"

    rm output
  fi
done;
