#!/bin/sh

set -xe

OWNER=$1
REPOSITORY=$2
shift 2

UPSTREAM=origin
DEFAULT_BRANCH=master

COMMIT_QUEUE_LABEL="commit-queue"
COMMIT_QUEUE_FAILED_LABEL="commit-queue-failed"

mergeUrl() {
  echo "repos/${OWNER}/${REPOSITORY}/pulls/${1}/merge"
}

commit_queue_failed() {
  pr=$1

  gh pr edit "$pr" --add-label "${COMMIT_QUEUE_FAILED_LABEL}"

  # shellcheck disable=SC2154
  cqurl="${GITHUB_SERVER_URL}/${OWNER}/${REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
  body="<details><summary>Commit Queue failed</summary><pre>$(cat output)</pre><a href='$cqurl'>$cqurl</a></details>"
  echo "$body"

  gh pr comment "$pr" --body "$body"

  rm output
}

# TODO(mmarchini): should this be set with whoever added the label for each PR?
git config --local user.email "github-bot@iojs.org"
git config --local user.name "Node.js GitHub Bot"

for pr in "$@"; do
  gh pr view "$pr" --json labels --jq ".labels" > labels.json
  # Skip PR if CI was requested
  if jq -e 'map(.name) | index("request-ci")' < labels.json; then
    echo "pr ${pr} skipped, waiting for CI to start"
    continue
  fi

  # Skip PR if CI is still running
  if ncu-ci url "https://github.com/${OWNER}/${REPOSITORY}/pull/${pr}" 2>&1 | grep "^Result *PENDING"; then
    echo "pr ${pr} skipped, CI still running"
    continue
  fi

  # Delete the commit queue label
  gh pr edit "$pr" --remove-label "$COMMIT_QUEUE_LABEL"

  if jq -e 'map(.name) | index("commit-queue-squash")' < labels.json; then
    MULTIPLE_COMMIT_POLICY="--fixupAll"
  elif jq -e 'map(.name) | index("commit-queue-rebase")' < labels.json; then
    MULTIPLE_COMMIT_POLICY=""
  else
    MULTIPLE_COMMIT_POLICY="--oneCommitMax"
  fi

  git node land --autorebase --yes $MULTIPLE_COMMIT_POLICY "$pr" >output 2>&1 || echo "Failed to land #${pr}"
  # cat here otherwise we'll be supressing the output of git node land
  cat output

  # TODO(mmarchini): workaround for ncu not returning the expected status code,
  # if the "Landed in..." message was not on the output we assume land failed
  if ! grep -q '. Post "Landed in .*/pull/'"${pr}" output; then
    commit_queue_failed "$pr"
    # If `git node land --abort` fails, we're in unknown state. Better to stop
    # the script here, current PR was removed from the queue so it shouldn't
    # interfere again in the future.
    git node land --abort --yes
    continue
  fi

  if [ -z "$MULTIPLE_COMMIT_POLICY" ]; then
    commits="$(git rev-parse $UPSTREAM/$DEFAULT_BRANCH)...$(git rev-parse HEAD)"

    if ! git push $UPSTREAM $DEFAULT_BRANCH >> output 2>&1; then
      commit_queue_failed "$pr"
      continue
    fi
  else
    # If there's only one commit, we can use the Squash and Merge feature from GitHub.
    # TODO: use `gh pr merge` when the GitHub CLI allows to customize the commit title (https://github.com/cli/cli/issues/1023).
    jq -n \
      --arg title "$(git log -1 --pretty='format:%s')" \
      --arg body "$(git log -1 --pretty='format:%b')" \
      --arg head "$(grep 'Fetched commits as' output | cut -d. -f3 | xargs git rev-parse)" \
      '{merge_method:"squash",commit_title:$title,commit_message:$body,sha:$head}' > output.json
    cat output.json
    if ! gh api -X PUT "$(mergeUrl "$pr")" --input output.json > output; then
      commit_queue_failed "$pr"
      continue
    fi
    cat output
    if ! commits="$(jq -r 'if .merged then .sha else error("not merged") end' < output)"; then
      commit_queue_failed "$pr"
      continue
    fi
    rm output.json
  fi

  rm output

  gh pr comment "$pr" --body "Landed in $commits"

  [ -z "$MULTIPLE_COMMIT_POLICY" ] && gh pr close "$pr"
done

rm -f labels.json
