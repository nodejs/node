#!/bin/sh

set -xe

UPSTREAM=origin
DEFAULT_BRANCH=main

COMMIT_QUEUE_LABEL="commit-queue"
COMMIT_QUEUE_FAILED_LABEL="commit-queue-failed"

cqurl="${GITHUB_SERVER_URL:?}/${GITHUB_REPOSITORY:?}/actions/runs/${GITHUB_RUN_ID:?}"

fence_code_block() {
  fence='```'
  while case $1 in *"$fence"*) ;; *) false ;; esac; do
    fence=$fence'`'
  done
  printf '%s\n%s\n%s\n' "$fence" "$1" "$fence"
}

format_diagnostics() {
  awk '
    function code_span(line, fence) {
      fence = "`"
      while (index(line, fence) != 0)
        fence = fence "`"
      return fence " " line " " fence
    }
    NR > 1 { printf "  \n" }
    { printf "%s", code_span($0) }
    END { if (NR > 0) printf "\n" }
  '
}

commit_queue_failed() {
  pr=$1
  reported_failure=${2:-}

  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --add-label "${COMMIT_QUEUE_FAILED_LABEL}" --remove-label "${COMMIT_QUEUE_LABEL}"

  last_output_line=$(awk 'NF { line = $0 } END { sub(/^[[:space:]]*/, "", line); print line }' output)
  missing_policy_message="ℹ  Add \`commit-queue-squash\` label to land the PR as one commit, or \`commit-queue-rebase\` to land as separate commits."
  if [ "$last_output_line" = "$missing_policy_message" ]; then
    failure_body='This pull request has multiple commits, but no landing policy was selected.

Add https://github.com/nodejs/node/labels/commit-queue-squash to land it as one commit, or https://github.com/nodejs/node/labels/commit-queue-rebase to land the commits separately.'
  else
    if [ -z "$reported_failure" ]; then
      reported_failure=$(grep -e '✘' -e '⚠' output | tail -n 10)
    fi
    if [ -z "$reported_failure" ]; then
      reported_failure=$(tail -n 10 output)
    fi
    if [ -z "$reported_failure" ]; then
      reported_failure='No failure reason was reported.'
    fi
    failure_body=$(printf '%s\n' "$reported_failure" | format_diagnostics)
  fi

  raw_output=$(cat output)
  full_output=$(fence_code_block "$raw_output")

  body="### Commit Queue failed

$failure_body

The pull request was removed from the Commit Queue and labeled https://github.com/nodejs/node/labels/commit-queue-failed. After resolving the failure, remove that label and add https://github.com/nodejs/node/labels/commit-queue to retry.

<details>
<summary>Full Commit Queue output</summary>

$full_output
</details>

[View workflow run]($cqurl)"
  echo "$body"

  gh -R "$GITHUB_REPOSITORY" pr comment "$pr" --body "$body"

  rm output
}

SHOULD_ABORT=

for pr in "$@"; do
  gh -R "$GITHUB_REPOSITORY" pr view "$pr" --json labels --jq ".labels" > labels.json
  
  if jq -e 'map(.name) | index("commit-queue-squash")' < labels.json; then
    MULTIPLE_COMMIT_POLICY="--fixupAll"
  elif jq -e 'map(.name) | index("commit-queue-rebase")' < labels.json; then
    MULTIPLE_COMMIT_POLICY=""
  else
    MULTIPLE_COMMIT_POLICY="--oneCommitMax"
  fi

  if [ -n "$SHOULD_ABORT" ]; then
    # If `git node land --abort` fails, we're in unknown state. Better to stop
    # the script here, current PR was removed from the queue so it shouldn't
    # interfere again in the future.
    git node land --abort --yes
    SHOULD_ABORT=
  fi

  git node land --autorebase --yes $MULTIPLE_COMMIT_POLICY "$pr" >output 2>&1 || echo "Failed to land #${pr}"
  # cat here otherwise we'll be suppressing the output of git node land
  cat output

  # TODO(mmarchini): workaround for ncu not returning the expected status code,
  # if the "Landed in..." message was not on the output we assume land failed
  if ! grep -q '. Post "Landed in .*/pull/'"${pr}" output; then
    commit_queue_failed "$pr"
    # Using a variable as there's no point in aborting if there are no PRs left in the queue.
    SHOULD_ABORT=1
    continue
  fi

  if [ -z "$MULTIPLE_COMMIT_POLICY" ]; then
    start_sha=$(git rev-parse $UPSTREAM/$DEFAULT_BRANCH)
    end_sha=$(git rev-parse HEAD)
    commits="${start_sha}...${end_sha}"

    if ! git push $UPSTREAM $DEFAULT_BRANCH >> output 2>&1; then
      commit_queue_failed "$pr" \
        "Failed to push the landed commits to ${UPSTREAM}/${DEFAULT_BRANCH}."
      continue
    fi
  else
    # If there's only one commit, we can use the Squash and Merge feature from GitHub.
    # TODO: use `gh pr merge` when the GitHub CLI allows to customize the commit title (https://github.com/cli/cli/issues/1023).
    commit_title=$(git log -1 --pretty='format:%s')
    commit_body=$(git log -1 --pretty='format:%b')
    commit_head=$(grep 'Fetched commits as' output | cut -d. -f3 | xargs git rev-parse)

    if ! commits="$(
      jq -cn \
        --arg title "${commit_title}" \
        --arg body "${commit_body}" \
        --arg head "${commit_head}" \
        '{merge_method:"squash",commit_title:$title,commit_message:$body,sha:$head}' |\
      gh api -X PUT "repos/${GITHUB_REPOSITORY}/pulls/${pr}/merge" --input -\
        --jq 'if .merged then .sha else halt_error end' 2>> output
    )"; then
      commit_queue_failed "$pr" \
        'GitHub failed to squash and merge this pull request.'
      continue
    fi
  fi

  rm output

  gh -R "$GITHUB_REPOSITORY" pr comment "$pr" --body "Landed in $commits"

  [ -z "$MULTIPLE_COMMIT_POLICY" ] && gh -R "$GITHUB_REPOSITORY" pr close "$pr"

  # Delete the commit queue label (but ignore errors, it's no big deal if a closed PR still has the label)
  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --remove-label "$COMMIT_QUEUE_LABEL" || true
done

rm -f labels.json
