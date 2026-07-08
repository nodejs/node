# Commit queue

_tl;dr: You can ask the queue to land pull requests by adding the
`commit-queue` label to them._

Commit Queue is a feature for the project which simplifies the
landing process by automating it via GitHub Actions. With it, collaborators can
queue pull requests for landing by adding the `commit-queue` label to a PR. The
selector checks readiness with `@node-core/utils`. If the pull request is only
blocked on a deferrable condition, currently wait time, the queue leaves the
label in place and retries later. Other failures continue to the existing
landing and failure-reporting path.

This document gives an overview of how the Commit Queue works, as well as
implementation details, reasoning for design choices, and current limitations.

## Overview

From a high-level, the Commit Queue works as follows:

1. Collaborators will add `commit-queue` label to pull requests they want the
   queue to land. The label can be added before the pull request has completed
   its wait time, or before requested CI has finished. Required approvals must
   already be in place. The commit queue does not request CI on its own.
2. On each scheduled run, the queue builds a candidate list from open pull
   requests with the `commit-queue` label and without the `blocked` label. The
   workflow uses a five-minute cron, but GitHub Actions scheduled workflows are
   not guaranteed to run exactly every five minutes. For each candidate, the
   queue will:
   1. Run a metadata-only readiness check that uses `@node-core/utils` without
      checking out the repository
   2. If the metadata check exits with a deferrable readiness code, meaning
      the PR is only blocked on wait time, keep the `commit-queue` label and
      skip this PR until a later queue run
   3. Check if the PR also has a `request-ci` label (if it has, skip this PR
      since it's pending a CI run)
   4. Check whether GitHub checks are still running (if they are, skip this PR)
   5. Remove the `commit-queue` label and run `git node land`
   6. If it fails:
      1. Add the `commit-queue-failed` label to the PR
      2. Leave a comment on the PR with the output from `git node land`
      3. Abort the `git node land` session. If the abort succeeds, continue to
         the next PR; otherwise, stop the queue in an unknown state
   7. If it succeeds:
      1. Push or merge the changes into nodejs/node
      2. Leave a comment on the PR with `Landed in ...`
      3. Close the PR
      4. Go to next PR in the queue

To make the Commit Queue squash all the commits of a pull request into the
first one, add the `commit-queue-squash` label.
To make the Commit Queue land a pull request containing several commits, add the
`commit-queue-rebase` label. When using this option, make sure
that all commits are self-contained, meaning every commit should pass all tests.

## Current limitations

The Commit Queue feature is still in early stages, and as such it might not
work for more complex pull requests. These are the currently known limitations
of the commit queue:

1. All commits in a pull request must either be following commit message
   guidelines or be a valid [`fixup!`](https://git-scm.com/docs/git-commit#Documentation/git-commit.txt---fixupamendrewordltcommitgt)
   commit that will be correctly handled by the [`--autosquash`](https://git-scm.com/docs/git-rebase#Documentation/git-rebase.txt---autosquash)
   option
2. A CI must have run and succeeded since the last change on the PR
3. A collaborator must have approved the PR since the last change
4. Only Jenkins CI and GitHub Actions are checked (V8 CI and CITGM are ignored)
5. The PR must target the `main` branch (PRs opened against other branches, such
   as backport PRs, are ignored)

## Implementation

The [action](../../.github/workflows/commit-queue.yml) runs on scheduled events.
It uses a five-minute cron because that is the smallest interval accepted by
GitHub Actions. Scheduled workflows are not guaranteed to run exactly at that
cadence and might take longer between runs.

The workflow also uses a concurrency group so only one commit queue run can be
active at a time. If a scheduled run starts while a previous run is still
running, GitHub Actions keeps at most one pending run for the same concurrency
group. A newer pending run replaces an older pending run.

Using the scheduler is preferable over using pull\_request\_target for two
reasons:

1. if two Commit Queue Actions execution overlap, there's a high-risk that
   the last one to finish will fail because the local branch will be out of
   sync with the remote after the first Action pushes. `issue_comment` event
   has the same limitation.
2. `pull_request_target` will only run if the Action exists on the base commit
   of a pull request, and it will run the Action version present on that
   commit, meaning we wouldn't be able to use it for already opened PRs
   without rebasing them first.

`@node-core/utils` is configured with a personal token and a Jenkins token from
[@nodejs-github-bot](https://github.com/nodejs/github-bot). The workflow starts
with a small selector step that uses GitHub CLI to fetch pull requests with the
`commit-queue` label. It first fetches the same age-based and fast-track buckets
the queue used before accepting early queue requests, then fetches the broader
queue and de-duplicates the result. This keeps not-yet-ready PRs from crowding
out PRs that the previous query would have selected if GitHub paginates or caps
a query result.

If there are candidate PRs, the selector installs `@node-core/utils`, downloads
the target branch's README without checking out the repository, and runs
`git node metadata --readme --json` for each candidate. This uses the same
`@node-core/utils` PR readiness checks as `git node land`, but does not clone,
fetch, or merge the PR. The selector consumes the structured metadata result
and its exit code instead of matching human-readable output:

* exit code `0`: the PR is ready and is passed to
  [`commit-queue.sh`](../../tools/actions/commit-queue.sh)
* exit codes `20`-`29`: the PR is not ready for a deferrable metadata reason,
  currently wait time, so it keeps the `commit-queue` label and is retried
  later
* exit codes `40`-`49`: the PR has a hard or mixed metadata readiness failure
  and is passed to [`commit-queue.sh`](../../tools/actions/commit-queue.sh)

The `20`-`29` exit code range is reserved by `@node-core/utils` for deferrable
metadata readiness states, and `40`-`49` is reserved for hard metadata failure
states. Unknown selector failures fail the workflow before starting the landing
job and leave PR labels unchanged so the queue can retry on a later scheduled
run. PRs passed through with exit code `40`-`49` continue through
`commit-queue.sh`. The script still applies its existing `request-ci` and
pending-check deferrals before removing the queue label and reporting a hard
failure.

> The personal token needs permission for public repositories and to read
> profiles. It is used by `@node-core/utils` and by the landing job for
> checkout, label and comment updates, merging, and pushing. Jenkins token is
> required to check CI status.

`commit-queue.sh` receives the following positional arguments:

1. The repository owner
2. The repository name
3. Every positional argument starting at this one will be a pull request ID of
   a pull request with commit-queue set.

The script will iterate over the pull requests. GitHub CLI is used to check if
the PR is waiting for CI to start (`request-ci` label) or still has pending
GitHub checks. The PR is skipped if CI is pending. No other CI validation is
done here since `git node land` will fail if the last CI failed.

The script removes the `commit-queue` label, then runs `git node land`,
forwarding stdout and stderr to a file. PRs that are only blocked on wait time
should have already been filtered by the metadata check. If a hard readiness
failure appears between the selector job and `git node land`, the landing job
adds a `commit-queue-failed` label to the PR, leaves a comment with the output
of `git node land`, and then aborts the landing session. If the abort fails,
the queue stops instead of continuing in an unknown state.

Fast-tracked PRs use the metadata check before the landing job. If the
fast-track request has not yet received enough collaborator thumbs-up, the queue
keeps the `commit-queue` label and retries until either the fast-track request
is approved or the PR becomes landable through the regular wait-time rules. The
commit queue does not create the fast-track request comment; that is handled
when the `fast-track` label is added. If that comment is missing, the queue
reports the failure instead of keeping the PR queued.

If no errors happen during `git node land`, the script either pushes the direct
rebase landing to `main` or uses GitHub's squash merge API for single-commit and
fixup landings. It then leaves a `Landed in ...` comment in the PR. GitHub
closes PRs merged through the merge API automatically; for direct pushes, the
script closes the PR. Iteration continues until all PRs have done the steps
above.

## Reverting broken commits

Reverting broken commits is done manually by collaborators, just like when
commits are landed manually via `git node land`. An easy way to revert is a
good feature for the project, but is not explicitly required for the Commit
Queue to work because the Action lands PRs just like collaborators do today. If
once we start using the Commit Queue we notice that the number of required
reverts increases drastically, we can pause the queue until a Revert Queue is
implemented, but until then we can enable the Commit Queue and then work on a
Revert Queue as a follow-up.
