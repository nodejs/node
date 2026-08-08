#!/bin/sh

# Keeps the `author ready` label in sync with the definition in
# doc/contributing/collaborator-guide.md. A pull request is author ready when:
#
#   * there is a CI run in progress or completed,
#   * there is at least one collaborator approval,
#   * there are no outstanding review comments.

set -e

# shellcheck source=tools/actions/pr-state.sh
. "$(dirname "$0")/pr-state.sh"

AUTHOR_READY_LABEL="author ready"

# shellcheck disable=SC2154
OWNER="${GH_REPO%%/*}"
REPO="${GH_REPO#*/}"

# shellcheck disable=SC2016  # `$owner` and friends are GraphQL variables
QUERY='
query($owner: String!, $repo: String!, $number: Int!) {
  repository(owner: $owner, name: $repo) {
    pullRequest(number: $number) {'"${PR_STATE_FRAGMENT}"'}
  }
}'

# shellcheck disable=SC2016  # `$ci_pattern` and friends are jq variables
ELIGIBLE="${PR_STATE_JQ}"'
.data.repository.pullRequest
| {
    missing: [
      if .state == "OPEN" then empty else "not open" end,
      if .isDraft then "draft" else empty end,
      if .mergeable == "CONFLICTING" then "merge conflict" else empty end,
      (labels[] | select(. == "blocked" or . == "wip") | "labelled \(.)"),
      (labels[] | select(. == "request-ci" or . == "request-ci-failed") | "CI not started yet"),
      if changes_requested then "changes requested" else empty end,
      if unresolved_threads then "unresolved review threads" else empty end,
      if (.latestOpinionatedReviews.nodes | any(.state == "APPROVED"))
        then empty else "not approved" end,

      # "A CI run in progress or completed": for a pull request that needs
      # Jenkins, only a Jenkins run counts; otherwise any check does.
      if (if jenkins_required then jenkins_ran($ci_pattern) else ci_started end)
        then empty else "no CI run" end,
      if ci_failing then "CI is failing" else empty end
    ],
    is_author_ready: has_label($author_ready_label)
  }'

for pr in "$@"; do
  state="$(
    gh api graphql -f query="${QUERY}" \
      -f owner="${OWNER}" -f repo="${REPO}" -F number="${pr}" |
      jq -c --arg ci_pattern "${CI_CONTEXT_PATTERN}" \
            --arg author_ready_label "${AUTHOR_READY_LABEL}" \
            "${ELIGIBLE}"
  )"

  missing="$(echo "${state}" | jq -r '.missing | join(", ")')"
  is_author_ready="$(echo "${state}" | jq -r '.is_author_ready')"

  if [ -z "${missing}" ] && [ "${is_author_ready}" = "false" ]; then
    echo "#${pr}: author ready, adding label"
    gh pr edit "${pr}" --add-label "${AUTHOR_READY_LABEL}"
  elif [ -n "${missing}" ] && [ "${is_author_ready}" = "true" ]; then
    echo "#${pr}: no longer author ready (${missing}), removing label"
    gh pr edit "${pr}" --remove-label "${AUTHOR_READY_LABEL}"
  elif [ -n "${missing}" ]; then
    echo "#${pr}: not author ready (${missing})"
  else
    echo "#${pr}: author ready, already labelled"
  fi
done
