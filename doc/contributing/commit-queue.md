# Commit queue

_tl;dr: You can ask the queue to land pull requests by adding the
`commit-queue` label to them._

Commit Queue is a feature for the project which simplifies the
landing process by automating it via GitHub Actions. With it, collaborators can
queue pull requests for landing by adding the `commit-queue` label to a PR. The
selector checks readiness with `@node-core/utils`. If the pull request is only
blocked on a deferrable condition, currently wait time, the queue leaves the
label in place and retries later. For pull requests that are at least two days
old and still waiting for a second approval, the queue adds the
`lacks-second-approval` label. The queue removes that label when it removes the
`commit-queue` label. Other failures continue to the existing landing and
failure-reporting path.

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
