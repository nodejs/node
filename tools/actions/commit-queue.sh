#!/bin/bash

set -xe

OWNER=$1
REPOSITORY=$2
GITHUB_TOKEN=$3
shift 3

API_URL=https://api.github.com
COMMIT_QUEUE_LABEL='commit-queue'
COMMIT_QUEUE_FAILED_LABEL='commit-queue-failed'

function prUrl() {
  echo "$API_URL/repos/${OWNER}/${REPOSITORY}/pulls/${1}"
}

function issueUrl() {
  echo "$API_URL/repos/${OWNER}/${REPOSITORY}/issues/${1}"
}

function labelsUrl() {
  echo "$(issueUrl "${1}")/labels"
}

function commentsUrl() {
  echo "$(issueUrl "${1}")/comments"
}

function gitHubCurl() {
  url=$1
  method=$2
  shift 2

  curl -fsL --request "$method" \
       --url "$url" \
       --header "authorization: Bearer ${GITHUB_TOKEN}" \
       --header 'content-type: application/json' "$@"
}


# TODO(mmarchini): should this be set with whoever added the label for each PR?
git config --local user.email "github-bot@iojs.org"
git config --local user.name "Node.js GitHub Bot"

for pr in "$@"; do
  # Skip PR if CI was requested
  if gitHubCurl "$(labelsUrl "$pr")" GET | jq -e 'map(.name) | index("request-ci")'; then
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

  git node land --autorebase --yes "$pr" >output 2>&1 || echo "Failed to land #${pr}"
  # cat here otherwise we'll be supressing the output of git node land
  cat output

  # TODO(mmarchini): workaround for ncu not returning the expected status code,
  # if the "Landed in..." message was not on the output we assume land failed
  if ! tail -n 10 output | grep '. Post "Landed in .*/pull/'"${pr}"; then
    gitHubCurl "$(labelsUrl "$pr")" POST --data '{"labels": ["'"${COMMIT_QUEUE_FAILED_LABEL}"'"]}'

    jq -n --arg content "<details><summary>Commit Queue failed</summary><pre>$(cat output)</pre></details>" '{body: $content}' > output.json
    cat output.json

    gitHubCurl "$(commentsUrl "$pr")" POST --data @output.json

    rm output output.json
    # If `git node land --abort` fails, we're in unknown state. Better to stop
    # the script here, current PR was removed from the queue so it shouldn't
    # interfer again in the future
    git node land --abort --yes
  else
    rm output
    head=$(gitHubCurl "$(prUrl "$pr")" GET | jq '{ ref: .head.ref, remote: .head.repo.ssh_url, can_modify: .maintainer_can_modify }')
    if [ "$(echo "$head" | jq -r .can_modify)" == "true" ]; then
      git push --force "$(echo "$head" | jq -r .remote)" master:"$(echo "$head" | jq -r .ref)"
      # Give GitHub time to sync before pushing to master
      sleep 5s
    fi
    git push origin master

    gitHubCurl "$(commentsUrl "$pr")" POST --data '{"body": "Landed in '"$(git rev-parse HEAD)"'"}'

    if [ "$(echo "$head" | jq -r .can_modify)" != "true" ]; then
      gitHubCurl "$(issueUrl "$pr")" PATCH --data '{"state": "closed"}'
    fi
  fi
done
