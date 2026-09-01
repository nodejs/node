# Commit queue

_tl;dr: You can ask the queue to land pull requests by adding the
`commit-queue` label to them._

Commit Queue simplifies the landing process by automating it with GitHub
Actions. Once a pull request is [author ready][] and its current CI has passed, a
collaborator can queue it for landing by adding the `commit-queue` label. A
second approval is not required before adding the label.

The queue checks readiness with `@node-core/utils`, including the required wait
time measured from when the pull request was opened:

* Pull requests with at least two approvals must be open for 48 hours.
* Pull requests with one approval must be open for seven days.
* Correctly approved [fast-track pull requests][] have no minimum wait time.

If wait time is the only unmet condition, the queue leaves the `commit-queue`
label in place and retries later. For queued pull requests that are at least two
days old and still waiting for a second approval, it also adds the
`lacks-second-approval` label. Another approval makes the pull request eligible
for the next queue run, provided that all other requirements remain satisfied.
The queue removes `lacks-second-approval` automatically when it removes
`commit-queue`; collaborators do not need to remove it manually.

Hard failures remove `commit-queue`, add `commit-queue-failed`, and post a
comment with the actionable failure reason and retry instructions. To resolve the
failure, remove `commit-queue-failed`, and add `commit-queue` to retry.

To make the Commit Queue squash all the commits of a pull request into the
first one, add the `commit-queue-squash` label.
To make the Commit Queue land a pull request containing several commits, add the
`commit-queue-rebase` label. When using this option, make sure
that all commits are self-contained, meaning every commit should pass all tests.

The implementation is in `commit-queue.yml` and `commit-queue.sh`.

## Current limitations

These are the currently known limitations of the commit queue:

1. All commits in a pull request must either be following commit message
   guidelines or be a valid [`fixup!`](https://git-scm.com/docs/git-commit#Documentation/git-commit.txt---fixupamendrewordltcommitgt)
   commit that will be correctly handled by the [`--autosquash`](https://git-scm.com/docs/git-rebase#Documentation/git-rebase.txt---autosquash)
   option.
2. A CI must have run and succeeded since the last change on the PR.
3. A collaborator must have approved the PR since the last change.
4. Only Jenkins CI and GitHub Actions are checked (V8 CI and CITGM are ignored).
5. The PR must target the `main` branch (PRs opened against other branches, such
   as backport PRs, are ignored).

[author ready]: ./collaborator-guide.md#author-ready-pull-requests
[fast-track pull requests]: ./collaborator-guide.md#waiting-for-approvals
