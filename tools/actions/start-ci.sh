#!/bin/sh

set -xe

REQUEST_CI_LABEL="request-ci"
REQUEST_CI_FAILED_LABEL="request-ci-failed"
cqurl="${GITHUB_SERVER_URL:?}/${GITHUB_REPOSITORY:?}/actions/runs/${GITHUB_RUN_ID:?}"

escape_code_block_or_line() {
  case $1 in
    *"
"*|'') fence='```' sep='
' ;;
    *[![:space:]]*) fence='`' sep=' ' ;;
    *) fence='`' sep='' ;;
  esac
  while case $1 in *"$fence"*) ;; *) false ;; esac; do
    fence=$fence'`'
  done
  printf '%s%s%s%s%s\n' "$fence" "$sep" "$1" "$sep" "$fence"
}

for pr in "$@"; do
  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --remove-label "$REQUEST_CI_LABEL"

  ci_started=yes
  rm -f output;
  ncu-ci run --check-for-duplicates "$pr" >output 2>&1 || ci_started=no
  cat output

  if [ "$ci_started" = "no" ]; then
    # Do we need to reset?
    gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --add-label "$REQUEST_CI_FAILED_LABEL"

    reported_failure=$(grep -e '✘' -e '✖' -e '⚠' -e 'ℹ' output | tail -n 10)
    if [ -z "$reported_failure" ]; then
      reported_failure=$(tail -n 10 output)
    fi
    if [ -z "$reported_failure" ]; then
      reported_failure='No failure reason was reported.'
    fi
    failure_body=$(escape_code_block_or_line "$reported_failure")
    raw_output=$(cat output)

    body="### Failed to start CI

$failure_body

<details>
<summary>Full Auto Start CI output</summary>

$(escape_code_block_or_line "$raw_output")

</details>

[View workflow run]($cqurl)"
    echo "$body"

    gh -R "$GITHUB_REPOSITORY" pr comment "$pr" --body "$body"

    rm output
  fi
done;
