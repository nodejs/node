#!/bin/sh

set -xe

GITHUB_TOKEN=$1
OWNER=$2
REPOSITORY=$3
API_URL=https://api.github.com
REQUEST_CI_LABEL='request-ci'
REQUEST_CI_FAILED_LABEL='request-ci-failed'
shift 3

issueUrl() {
  echo "$API_URL/repos/${OWNER}/${REPOSITORY}/issues/${1}"
}

labelsUrl() {
  echo "$(issueUrl "${1}")/labels"
}

commentsUrl() {
  echo "$(issueUrl "${1}")/comments"
}

for pr in "$@"; do
  curl -sL --request DELETE \
       --url "$(labelsUrl "$pr")"/"$REQUEST_CI_LABEL" \
       --header "authorization: Bearer ${GITHUB_TOKEN}" \
       --header 'content-type: application/json'

  ci_started=yes
  rm -f output;
  ncu-ci run "$pr" >output 2>&1 || ci_started=no
  cat output

  if [ "$ci_started" = "no" ]; then
    # Do we need to reset?
    curl -sL --request PUT \
       --url "$(labelsUrl "$pr")" \
       --header "authorization: Bearer ${GITHUB_TOKEN}" \
       --header 'content-type: application/json' \
       --data '{"labels": ["'"${REQUEST_CI_FAILED_LABEL}"'"]}'

    jq -n --arg content "<details><summary>Couldn't start CI</summary><pre>$(cat output)</pre></details>" '{body: $content}' > output.json

    curl -sL --request POST \
       --url "$(commentsUrl "$pr")" \
       --header "authorization: Bearer ${GITHUB_TOKEN}" \
       --header 'content-type: application/json' \
       --data @output.json

    rm output.json;
  fi
done;
