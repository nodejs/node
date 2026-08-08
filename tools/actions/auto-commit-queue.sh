#!/bin/sh

# Adds `commit-queue` to pull requests that are ready to land, so nobody has to
# come back once the waiting period is over.
#
#   * open, not a draft, targets `main`, and `mergeable` is not CONFLICTING
#   * at least one approval given against the commit that would land
#   * semver-major needs two of those approvals to be from TSC voting members
#   * two approvals and 48 hours since it was opened (unless fast-track)
#   * every GitHub check passing
#   * a green Jenkins run when the pull request is labelled `needs-ci`
#   * that CI having run in the last five days, so `main` has not moved on

set -e

# shellcheck source=tools/actions/pr-state.sh
. "$(dirname "$0")/pr-state.sh"

COMMIT_QUEUE_LABEL="commit-queue"

TSC_MEMBERS="$(
  sed -n '/^#### TSC voting members$/,/^#### TSC regular members$/ s/^\* \[\([^]]*\)\].*/\1/p' \
    "$(dirname "$0")/../../README.md" |
    tr '[:upper:]' '[:lower:]' |
    sort -u |
    jq -Rsc 'split("\n") | map(select(length > 0))'
)"

# shellcheck disable=SC2154
SEARCH="repo:${GH_REPO} is:pr is:open \
label:\"author ready\" \
-label:blocked -label:wip \
-label:${COMMIT_QUEUE_LABEL} -label:${COMMIT_QUEUE_LABEL}-failed \
-label:request-ci -label:request-ci-failed"

# shellcheck disable=SC2016  # `$q` is a GraphQL variable
QUERY='
query($q: String!) {
  search(query: $q, type: ISSUE, first: 50) {
    nodes {
      ... on PullRequest {'"${PR_STATE_FRAGMENT}"'}
    }
  }
}'

# shellcheck disable=SC2016  # `$tsc` and friends are jq variables
READY="${PR_STATE_JQ}"'
[ .data.search.nodes[]
  | select(. != null)
  | (current_approvals) as $approvals
  | ([$approvals[].author.login | ascii_downcase] | map(select(IN($tsc[]))) | length) as $tsc_approvals
  | (if has_label("semver-major") and ($tsc | length) > 0 then $tsc_approvals >= 2
     elif has_label("semver-major") then ($approvals | length) >= 2
     else ($approvals | length) >= 1 end) as $approved
  | (if has_label("fast-track") then ($approvals | length) >= 2
     elif wait_waived then true
     else (($approvals | length) >= 2 and age_seconds >= $wait_multi)
          or age_seconds >= $wait_single
     end) as $waited
  | select(
      $approved and $waited
      and .state == "OPEN"
      and (.isDraft | not)
      and .baseRefName == "main"
      and .mergeable != "CONFLICTING"
      and (changes_requested | not)
      and (unresolved_threads | not)
      and github_checks_passing
      and ((jenkins_required | not) or jenkins_passing($ci_pattern))
      and ci_recent($ci_pattern; $ci_max_age)
    )
  | .number
]'

numbers="$(
  gh api graphql -f query="${QUERY}" -f q="${SEARCH}" |
    jq -r --arg ci_pattern "${CI_CONTEXT_PATTERN}" \
          --argjson tsc "${TSC_MEMBERS:-[]}" \
          --argjson wait_multi "${WAIT_MULTI_APPROVAL}" \
          --argjson wait_single "${WAIT_SINGLE_APPROVAL}" \
          --argjson ci_max_age "${CI_MAX_AGE}" \
          "${READY} | .[]"
)"

if [ -z "${numbers}" ]; then
  echo "No pull requests are ready for the commit queue."
  exit 0
fi

for pr in ${numbers}; do
  echo "#${pr}: ready to land, adding ${COMMIT_QUEUE_LABEL}"
  gh pr edit "${pr}" --add-label "${COMMIT_QUEUE_LABEL}"
done
