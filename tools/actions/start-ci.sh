#!/bin/sh

set -xe

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
  request_labels=$(gh -R "$GITHUB_REPOSITORY" pr view "$pr" --json labels \
    --jq '[.labels[].name | select(. == "request-ci" or . == "resume-ci")] | sort | join(",")')
  case "$request_labels" in
    request-ci)
      action=start
      failed_labels=request-ci-failed
      ;;
    resume-ci)
      action=resume
      failed_labels=resume-ci-failed
      ;;
    request-ci,resume-ci)
      action='start or resume'
      failed_labels=request-ci-failed,resume-ci-failed
      ;;
    *) continue ;;
  esac
  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --remove-label "$request_labels"

  ci_succeeded=yes
  rm -f output;
  if [ "$request_labels" = "request-ci,resume-ci" ]; then
    echo 'Refusing to start or resume CI while both request-ci and resume-ci labels are present' >output
    ci_succeeded=no
  elif [ "$action" = "resume" ]; then
    ncu-ci resume "$pr" >output 2>&1 || ci_succeeded=no
  else
    ncu-ci run --check-for-duplicates "$pr" >output 2>&1 || ci_succeeded=no
  fi
  cat output

  if [ "$ci_succeeded" = "no" ]; then
    gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --add-label "$failed_labels"

    reported_failure=$(grep -e '✘' -e '✖' -e '⚠' -e 'ℹ' output | tail -n 10)
    if [ -z "$reported_failure" ]; then
      reported_failure=$(tail -n 10 output)
    fi
    if [ -z "$reported_failure" ]; then
      reported_failure='No failure reason was reported.'
    fi
    failure_body=$(escape_code_block_or_line "$reported_failure")
    raw_output=$(cat output)

    body="### Failed to $action CI

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
