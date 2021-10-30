#!/bin/sh

set -xe

OWNER=$1
REPOSITORY=$2
GITHUB_TOKEN=$3
shift 3

UPSTREAM=origin
DEFAULT_BRANCH=master

API_URL=https://api.github.com
COMMIT_QUEUE_LABEL='commit-queue'
COMMIT_QUEUE_FAILED_LABEL='commit-queue-failed'

issueUrl() {
  echo "$API_URL/repos/${OWNER}/${REPOSITORY}/issues/${1}"
}

mergeUrl() {
  echo "$API_URL/repos/${OWNER}/${REPOSITORY}/pulls/${1}/merge"
}

labelsUrl() {
  echo "$(issueUrl "${1}")/labels"
}

commentsUrl() {
  echo "$(issueUrl "${1}")/comments"
}

gitHubCurl() {
  url=$1
  method=$2
  shift 2

  curl -fsL --request "$method" \
       --url "$url" \
       --header "authorization: Bearer ${GITHUB_TOKEN}" \
       --header 'content-type: application/json' "$@"
}

commit_queue_failed() {
  gitHubCurl "$(labelsUrl "${1}")" POST --data '{"labels": ["'"${COMMIT_QUEUE_FAILED_LABEL}"'"]}'

  # shellcheck disable=SC2154
  cqurl="${GITHUB_SERVER_URL}/${OWNER}/${REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
  jq -n --arg content "<details><summary>Commit Queue failed</summary><pre>$(cat output)</pre><a href='$cqurl'>$cqurl</a></details>" '{body: $content}' > output.json
  cat output.json

  gitHubCurl "$(commentsUrl "${1}")" POST --data @output.json

  rm output output.json
}

# TODO(mmarchini): should this be set with whoever added the label for each PR?
git config --local user.email "github-bot@iojs.org"
git config --local user.name "Node.js GitHub Bot"

for pr in "$@"; do
  gitHubCurl "$(labelsUrl "$pr")" GET > labels.json
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
  gitHubCurl "$(labelsUrl "$pr")"/"$COMMIT_QUEUE_LABEL" DELETE

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
    # If there's only one commit, we can use the Squash and Merge feature from GitHub
    jq -n \
      --arg title "$(git log -1 --pretty='format:%s')" \
      --arg body "$(git log -1 --pretty='format:%b')" \
      '{merge_method:"squash",commit_title:$title,commit_message:$body}' > output.json
    cat output.json
    gitHubCurl "$(mergeUrl "$pr")" PUT --data @output.json > output
    cat output
    if ! commits="$(jq -r 'if .merged then .sha else error("not merged") end' < output)"; then
      commit_queue_failed "$pr"
      continue
    fi
    rm output.json
  fi

  rm output

  gitHubCurl "$(commentsUrl "$pr")" POST --data '{"body": "Landed in '"$commits"'"}'

  [ -z "$MULTIPLE_COMMIT_POLICY" ] && gitHubCurl "$(issueUrl "$pr")" PATCH --data '{"state": "closed"}'
done

rm -f labels.json
