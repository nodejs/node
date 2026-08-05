#!/bin/sh
# shellcheck disable=SC2034  # everything here is read by the scripts that source it

CI_CONTEXT_PATTERN="${CI_CONTEXT_PATTERN:-node-test-pull-request}"
WAIT_MULTI_APPROVAL="${WAIT_MULTI_APPROVAL:-172800}"
WAIT_SINGLE_APPROVAL="${WAIT_SINGLE_APPROVAL:-604800}"

PR_STATE_FRAGMENT='
  number
  state
  isDraft
  mergeable
  createdAt
  baseRefName
  labels(first: 100) { nodes { name } }
  latestOpinionatedReviews(first: 100, writersOnly: true) {
    nodes { state author { login } commit { oid } }
  }
  reviewThreads(first: 100) {
    nodes { isResolved isOutdated }
  }
  commits(last: 1) {
    nodes {
      commit {
        oid
        statusCheckRollup {
          contexts(first: 100) {
            nodes {
              ... on StatusContext { context state }
              ... on CheckRun { name conclusion }
            }
          }
        }
      }
    }
  }
'

# shellcheck disable=SC2016  # `$name` and friends are jq parameters
PR_STATE_JQ='
def labels: .labels.nodes | map(.name);
def has_label($name): labels | index($name) != null;
def changes_requested: .latestOpinionatedReviews.nodes | any(.state == "CHANGES_REQUESTED");
def unresolved_threads: .reviewThreads.nodes | any((.isResolved | not) and (.isOutdated | not));
def head_commit: .commits.nodes[0].commit;

# Only approvals given against the commit that would actually land count
def current_approvals:
  # Bound before the map, where `.` is still the pull request rather than a review.
  head_commit.oid as $oid
  | .latestOpinionatedReviews.nodes
  | map(select(.state == "APPROVED" and .commit.oid == $oid));

def check_results: (head_commit.statusCheckRollup.contexts.nodes // []) | map(.conclusion // .state);
def ci_started: (check_results | length) > 0;
def ci_failing:
  check_results
  | any(IN("FAILURE", "ERROR", "TIMED_OUT", "CANCELLED", "ACTION_REQUIRED", "STARTUP_FAILURE"));
# PRChecker treats neutral and skipped checks as passing.
def github_checks_passing: ci_started and (check_results | all(IN("SUCCESS", "NEUTRAL", "SKIPPED")));

def jenkins_contexts($pattern):
  (head_commit.statusCheckRollup.contexts.nodes // [])
  | map(select((.context // .name) | test($pattern; "i")));
def jenkins_ran($pattern): (jenkins_contexts($pattern) | length) > 0;
def jenkins_passing($pattern):
  jenkins_ran($pattern) and (jenkins_contexts($pattern) | all((.conclusion // .state) == "SUCCESS"));
def jenkins_required: has_label("needs-ci");

def age_seconds: now - (.createdAt | fromdateiso8601);
def wait_waived: has_label("fast-track");
'
