# How to Backport a Pull Request to a Release Line

## Staging branches

Each release line has a staging branch that the releaser will use as a scratch
pad while preparing a release. The branch name is formatted as follows:
`vN.x-staging` where `N` is the major release number.

*Note*: For the active staging branches see the [LTS Schedule][].

## What needs to be backported?

If a cherry-pick from master does not land cleanly on a staging branch, the
releaser will mark the pull request with a particular label for that release
line (e.g. `backport-requested-vN.x`), specifying to our tooling that this
pull request should not be included. The releaser will then add a comment
requesting that a backport pull request be made.

## What can be backported?

The "Current" release line is much more lenient than the LTS release lines in
what can be landed. Our LTS release lines (see the [LTS Plan][])
require that commits mature in the Current release for at least 2 weeks before
they can be landed in an LTS staging branch. Only after "maturation" will those
commits be cherry-picked or backported.

## How to submit a backport pull request

For the following steps, let's assume that a backport is needed for the v6.x
release line. All commands will use the `v6.x-staging` branch as the target
branch. In order to submit a backport pull request to another branch, simply
replace that with the staging branch for the targeted release line.

1. Checkout the staging branch for the targeted release line
2. Make sure that the local staging branch is up to date with the remote
3. Create a new branch off of the staging branch

```shell
# Assuming your fork of Node.js is checked out in $NODE_DIR,
# the origin remote points to your fork, and the upstream remote points
# to git://github.com/nodejs/node
cd $NODE_DIR
# If v6.x-staging is checked out `pull` should be used instead of `fetch`
git fetch upstream v6.x-staging:v6.x-staging -f
# Assume we want to backport PR #10157
git checkout -b backport-10157-to-v6.x v6.x-staging
```

4. After creating the branch, apply the changes to the branch. The cherry-pick
   will likely fail due to conflicts. In that case, you will see something
   like this:

```shell
# Say the $SHA is 773cdc31ef
$ git cherry-pick $SHA # Use your commit hash
error: could not apply 773cdc3... <commit title>
hint: after resolving the conflicts, mark the corrected paths
hint: with 'git add <paths>' or 'git rm <paths>'
hint: and commit the result with 'git commit'
```

5. Make the required changes to remove the conflicts, add the files to the index
   using `git add`, and then commit the changes. That can be done with
   `git cherry-pick --continue`.
6. Leave the commit message as is. If you think it should be modified, comment
   in the Pull Request.
7. Make sure `make -j4 test` passes.
8. Push the changes to your fork
9. Open a pull request:
   1. Be sure to target the `v6.x-staging` branch in the pull request.
   2. Include the backport target in the pull request title in the following
      format — `[v6.x backport] <commit title>`.
      Example: `[v6.x backport] process: improve performance of nextTick`
   3. Check the checkbox labelled "Allow edits from maintainers".
   4. In the description add a reference to the original PR
   5. Run a [`node-test-pull-request`][] CI job (with `REBASE_ONTO` set to the
      default `<pr base branch>`)
10. If during the review process conflicts arise, use the following to rebase:
    `git pull --rebase upstream v6.x-staging`

After the PR lands replace the `backport-requested-v6.x` label on the original
PR with `backported-to-v6.x`.

[LTS Schedule]: https://github.com/nodejs/LTS/#lts-schedule1
[LTS Plan]: https://github.com/nodejs/LTS#lts-plan
[`node-test-pull-request`]: https://ci.nodejs.org/job/node-test-pull-request/build
