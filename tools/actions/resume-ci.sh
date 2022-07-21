#!/bin/sh

set -xe

RESUME_CI_LABEL="resume-ci"
RESUME_CI_FAILED_LABEL="resume-ci-failed"

for pr in "$@"; do
  gh pr edit "$pr" --remove-label "$RESUME_CI_LABEL"

  ci_started=yes
  rm -f output;
  ncu-ci resume "$pr" >output 2>&1 || ci_started=no
  cat output

  if [ "$ci_started" = "no" ]; then
    # Do we need to reset?
    gh pr edit "$pr" --add-label "$RESUME_CI_FAILED_LABEL"

    jq -n --arg content "<details><summary>Couldn't resume CI</summary><pre>$(cat output || true)</pre></details>" > output.json

    gh pr comment "$pr" --body-file output.json

    rm output.json;
  fi
done;
