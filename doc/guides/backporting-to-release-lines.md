# How to Backport a Pull Request to a Release Line

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

If a cherry-pick from master does not land cleanly on a staging branch, the
releaser will mark the pull request with a particular label for that release
line, specifying to our tooling that this pull request should not be included.
The releaser will then add a comment that a backport is needed.

## What can be backported?

The "Current" release line is much more lenient than the LTS release lines in
what can be landed. Our LTS release lines (v4.x and v6.x as of March 2017)
require that commits mature in a Current release for at least 2 weeks before
they can be landed on staging. If the commit can not be cherry-picked from
master a manual backport will need to be submitted. Please see the [LTS Plan][]
for more information. After that time, if the commit can be cherry-picked
cleanly from master, then nothing needs to be done. If not, a backport pull
request will need to be submitted.

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
# Fails if you already have a v7.x-staging
git branch v7.x-staging upstream/v7.x-staging
git checkout v7.x-staging
git reset --hard upstream/v7.x-staging
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

* Make the required changes to remove the conflicts, add the files to the index
  using `git add`, and then commit the changes. That can be done with
  `git cherry-pick --continue`.
* Leave the commit message as is. If you think it should be modified, comment
  in the Pull Request.
* Make sure `make -j4 test` passes
* Push the changes to your fork and open a pull request.
* Be sure to target the `v7.x-staging` branch in the pull request.

### Helpful Hints

* Please include the backport target in the pull request title in the following
  format: `(v7.x backport) <commit title>`
  * Ex. `(v4.x backport) process: improve performance of nextTick`
* Please check the checkbox labelled "Allow edits from maintainers".
  This is the easiest way to to avoid constant rebases.

In the event the backport pull request is different than the original,
the backport pull request should be reviewed the same way a new pull request
is reviewed.

[LTS Plan]: https://github.com/nodejs/LTS#lts-plan
