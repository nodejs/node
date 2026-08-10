#!/bin/sh

set -xe

UPSTREAM=origin
DEFAULT_BRANCH=main

COMMIT_QUEUE_LABEL="commit-queue"
COMMIT_QUEUE_FAILED_LABEL="commit-queue-failed"

cqurl="${GITHUB_SERVER_URL:?}/${GITHUB_REPOSITORY:?}/actions/runs/${GITHUB_RUN_ID:?}"

commit_queue_failed() {
  pr=$1

  gh -R "$GITHUB_REPOSITORY" pr edit "$pr" --add-label "${COMMIT_QUEUE_FAILED_LABEL}" --remove-label "${COMMIT_QUEUE_LABEL}"

  body="<details><summary>Commit Queue failed</summary><pre>$(sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' output)</pre><a href='$cqurl'>$cqurl</a></details>"
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
      commit_queue_failed "$pr"
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
        --jq 'if .merged then .sha else halt_error end'
    )"; then
      commit_queue_failed "$pr"
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
