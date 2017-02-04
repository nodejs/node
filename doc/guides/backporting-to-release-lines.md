# How to backport a Pull Request to a Release Line

## Staging branches

Each release line has a staging branch that the releaser will use as a scratch
pad while preparing a release. The branch name is formatted as follows:
`vN.x-staging` where `N` is the major release number.

### Active staging branches

| Release Line | Staging Branch |
| ------------ | -------------- |
| `v7.x`       | `v7.x-staging` |
| `v6.x`       | `v6.x-staging` |
| `v4.x`       | `v4.x-staging` |

## What needs to be backported?

When a release is being prepared, the releaser attempts to cherry-pick a
certain set of commits from the master branch to the release staging branch.
The criteria for consideration depends on the target version (Current vs. LTS).
If a cherry-pick does not land cleanly on the staging branch, the releaser
will mark the pull request with a particular label for that release line,
specifying to our tooling that this pull request should not be included. The
releaser will then add a comment that a backport is needed.

## What can be backported?

The "Current" release line (currently v7.x) is much more lenient than the LTS
release lines in what can be landed. Our LTS release lines
(currently v4.x and v6.x) require that commits live in a Current release for at
least 2 weeks before backporting. Please see the [`LTS Plan`][] for more
information. After that time, if the commit can be cherry-picked cleanly from
master, then nothing needs to be done. If not, a backport pull request will
need to be submitted.

## How to submit a backport pull request

For these steps, let's assume that a backport is needed for the v7.x release
line. All commands will use the v7.x-staging branch as the target branch.
In order to submit a backport pull request to another branch, simply replace
that with the staging branch for the targeted release line.

* Checkout the staging branch for the targeted release line
* Make sure that the local staging branch is up to date with the remote
* Create a new branch off of the staging branch

```shell
# Assuming your fork of Node.js is checked out in $NODE_DIR,
# the origin remote points to your fork, and the upstream remote points
# to git://github.com/nodejs/node
cd $NODE_DIR
git checkout v7.x-staging
git pull -r upstream v7.x-staging
# We want to backport pr #10157
git checkout -b backport-10157-to-v7.x
```

* After creating the branch, apply the changes to the branch. The cherry-pick
  will likely fail due to conflicts. In that case, you will see something this:

```shell
# Say the $SHA is 773cdc31ef
$ git cherry-pick $SHA # Use your commit hash
error: could not apply 773cdc3... <commit title>
hint: after resolving the conflicts, mark the corrected paths
hint: with 'git add <paths>' or 'git rm <paths>'
hint: and commit the result with 'git commit'
```

* Make the required changes to remove the conflicts, and then commit the
  changes. That can be done with `git commit`.
* The commit message should be as close as possible to the commit message on the
  master branch, unless the commit has to be different due to dependencies that
  are not present in the targeted release line. The only exception is that the
  metadata from the original commit should be removed. If a backport is
  required, it should go through the same review steps as a commit landing
  in the master branch.
* Push the changes to your fork and open a pull request.
* Be sure to target the `v7.x-staging` branch in the pull request.

### Helpful Hints

* Please include the backport target in the pull request title in the following
  format: `(v7.x backport) <commit title>`
  * Ex. `(v4.x backport) process: improve performance of nextTick`
* Please include the text `Backport of #<pull request number>` in the
  pull request description. This will link to the original pull request.

In the event the backport pull request is different than the original,
the backport pull request should be reviewed the same way a new pull request
is reviewed. When each commit is landed, the new reviewers and the new PR-URL
should be used.

[`LTS Plan`]: https://github.com/nodejs/LTS#lts-plan
