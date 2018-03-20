# Landing Pull Requests

* [Using `git-node`](#using-git-node)
* [Technical HOWTO](#technical-howto)
* [Troubleshooting](#troubleshooting)
* [I Just Made a Mistake](#i-just-made-a-mistake)
* [Long Term Support](#long-term-support)
  * [What is LTS?](#what-is-lts)
  * [How does LTS work?](#how-does-lts-work)
  * [Landing semver-minor commits in LTS](#landing-semver-minor-commits-in-lts)
  * [How are LTS Branches Managed?](#how-are-lts-branches-managed)
  * [How can I help?](#how-can-i-help)
  * [How is an LTS release cut?](#how-is-an-lts-release-cut)

## Important notes
* Please never use GitHub's green ["Merge Pull Request"](https://help.github.com/articles/merging-a-pull-request/#merging-a-pull-request-on-github) button.
  * If you do, please force-push removing the merge.
  * Reasons for not using the web interface button:
    * The merge method will add an unnecessary merge commit.
    * The squash & merge method has been known to add metadata to the
    commit title (the PR #).
    * If more than one author has contributed to the PR, keep the most recent
      author when squashing.
* Review the commit message to ensure that it adheres to the guidelines outlined
in the [contributing](/doc/guides/contributing/pull-requests.md#commit-message-guidelines) guide.
* Add all necessary [metadata](#metadata) to commit messages before landing.
* See the commit log for examples such as
[this one](https://github.com/nodejs/node/commit/b636ba8186) if unsure
exactly how to format your commit messages.
* Additionally:
  * Double check PRs to make sure the person's _full name_ and email
  address are correct before merging.
  * All commits should be self-contained (meaning every commit should pass all
  tests). This makes it much easier when bisecting to find a breaking change.

## Using `git-node`

In most cases, using [the `git-node` command][git-node] of [`node-core-utils`][]
should be enough to help you land a Pull Request. If you discover a problem when
using this tool, please file an issue
[to the issue tracker][node-core-utils-issues].

Quick example:

```text
$ npm install -g node-core-utils
$ git node land $PRID
```

If it's the first time you have used `node-core-utils`, you will be prompted
to type the password of your GitHub account and the two-factor authentication
code in the console so the tool can create the GitHub access token for you.
If you do not want to do that, follow
[the `node-core-utils` guide][node-core-utils-credentials]
to set up your credentials manually.

## Technical HOWTO

Clear any `am`/`rebase` that may already be underway:

```text
$ git am --abort
$ git rebase --abort
```

Checkout proper target branch:

```text
$ git checkout master
```

Update the tree (assumes your repo is set up as detailed in
[CONTRIBUTING.md](/doc/guides/contributing/pull-requests.md#step-1-fork)):

```text
$ git fetch upstream
$ git merge --ff-only upstream/master
```

Apply external patches:

```text
$ curl -L https://github.com/nodejs/node/pull/xxx.patch | git am --whitespace=fix
```

If the merge fails even though recent CI runs were successful, then a 3-way
merge may be required.  In this case try:

```text
$ git am --abort
$ curl -L https://github.com/nodejs/node/pull/xxx.patch | git am -3 --whitespace=fix
```
If the 3-way merge succeeds you can proceed, but make sure to check the changes
against the original PR carefully and build/test on at least one platform
before landing. If the 3-way merge fails, then it is most likely that a
conflicting PR has landed since the CI run and you will have to ask the author
to rebase.

Check and re-review the changes:

```text
$ git diff upstream/master
```

Check the number of commits and commit messages:

```text
$ git log upstream/master...master
```

Squash commits and add metadata:

```text
$ git rebase -i upstream/master
```

This will open a screen like this (in the default shell editor):

```text
pick 6928fc1 crypto: add feature A
pick 8120c4c add test for feature A
pick 51759dc feature B
pick 7d6f433 test for feature B

# Rebase f9456a2..7d6f433 onto f9456a2
#
# Commands:
#  p, pick = use commit
#  r, reword = use commit, but edit the commit message
#  e, edit = use commit, but stop for amending
#  s, squash = use commit, but meld into previous commit
#  f, fixup = like "squash", but discard this commit's log message
#  x, exec = run command (the rest of the line) using shell
#
# These lines can be re-ordered; they are executed from top to bottom.
#
# If you remove a line here THAT COMMIT WILL BE LOST.
#
# However, if you remove everything, the rebase will be aborted.
#
# Note that empty commits are commented out
```

Replace a couple of `pick`s with `fixup` to squash them into a
previous commit:

```text
pick 6928fc1 crypto: add feature A
fixup 8120c4c add test for feature A
pick 51759dc feature B
fixup 7d6f433 test for feature B
```

Replace `pick` with `reword` to change the commit message:

```text
reword 6928fc1 crypto: add feature A
fixup 8120c4c add test for feature A
reword 51759dc feature B
fixup 7d6f433 test for feature B
```

Save the file and close the editor. You'll be asked to enter a new
commit message for that commit. This is a good moment to fix incorrect
commit logs, ensure that they are properly formatted, and add
`Reviewed-By` lines.

* The commit message text must conform to the
[commit message guidelines](/doc/guides/contributing/pull-requests.md#commit-message-guidelines).

<a name="metadata"></a>
* Modify the original commit message to include additional metadata regarding
  the change process. (The [`git node metadata`][git-node-metadata] command
  can generate the metadata for you.)

  * Required: A `PR-URL:` line that references the *full* GitHub URL of the
    original pull request being merged so it's easy to trace a commit back to
    the conversation that led up to that change.
  * Optional: A `Fixes: X` line, where _X_ either includes the *full* GitHub URL
    for an issue, and/or the hash and commit message if the commit fixes
    a bug in a previous commit. Multiple `Fixes:` lines may be added if
    appropriate.
  * Optional: One or more `Refs:` lines referencing a URL for any relevant
    background.
  * Required: A `Reviewed-By: Name <email>` line for yourself and any
    other Collaborators who have reviewed the change.
    * Useful for @mentions / contact list if something goes wrong in the PR.
    * Protects against the assumption that GitHub will be around forever.

Run tests (`make -j4 test` or `vcbuild test`). Even though there was a
successful continuous integration run, other changes may have landed on master
since then, so running the tests one last time locally is a good practice.

Validate that the commit message is properly formatted using
[core-validate-commit](https://github.com/evanlucas/core-validate-commit).

```text
$ git rev-list upstream/master...HEAD | xargs core-validate-commit
```

Optional: When landing your own commits, force push the amended commit to the
branch you used to open the pull request. If your branch is called `bugfix`,
then the command would be `git push --force-with-lease origin master:bugfix`.
When the pull request is closed, this will cause the pull request to
show the purple merged status rather than the red closed status that is
usually used for pull requests that weren't merged.

Time to push it:

```text
$ git push upstream master
```

Close the pull request with a "Landed in `<commit hash>`" comment. If
your pull request shows the purple merged status then you should still
add the "Landed in <commit hash>..<commit hash>" comment if you added
multiple commits.

## Troubleshooting

Sometimes, when running `git push upstream master`, you may get an error message
like this:

```console
To https://github.com/nodejs/node
 ! [rejected]              master -> master (fetch first)
error: failed to push some refs to 'https://github.com/nodejs/node'
hint: Updates were rejected because the remote contains work that you do
hint: not have locally. This is usually caused by another repository pushing
hint: to the same ref. You may want to first integrate the remote changes
hint: (e.g. 'git pull ...') before pushing again.
hint: See the 'Note about fast-forwards' in 'git push --help' for details.
```

That means a commit has landed since your last rebase against `upstream/master`.
To fix this, fetch, rebase, run the tests again (to make sure no interactions
between your changes and the new changes cause any problems), and push again:

```sh
git fetch upstream
git rebase upstream/master
make -j4 test
git push upstream master
```

## I Just Made a Mistake

* Ping a TSC member.
* `#node-dev` on freenode
* With `git`, there's a way to override remote trees by force pushing
(`git push -f`). This should generally be seen as forbidden (since
you're rewriting history on a repository other people are working
against) but is allowed for simpler slip-ups such as typos in commit
messages. However, you are only allowed to force push to any Node.js
branch within 10 minutes from your original push. If someone else
pushes to the branch or the 10 minute period passes, consider the
commit final.
  * Use `--force-with-lease` to minimize the chance of overwriting
  someone else's change.
  * Post to `#node-dev` (IRC) if you force push.

## Long Term Support

## What is LTS?

Long Term Support (often referred to as *LTS*) guarantees application developers
a 30-month support cycle with specific versions of Node.js.

You can find more information
[in the full release plan](https://github.com/nodejs/Release#release-plan).

## How does LTS work?

Once a Current branch enters LTS, changes in that branch are limited to bug
fixes, security updates, possible npm updates, documentation updates, and
certain performance improvements that can be demonstrated to not break existing
applications. Semver-minor changes are only permitted if required for bug fixes
and then only on a case-by-case basis with LTS WG and possibly Technical
Steering Committee (TSC) review. Semver-major changes are permitted only if
required for security-related fixes.

Once a Current branch moves into Maintenance mode, only **critical** bugs,
**critical** security fixes, and documentation updates will be permitted.

## Landing semver-minor commits in LTS

The default policy is to not land semver-minor or higher commits in any LTS
branch. However, the LTS WG or TSC can evaluate any individual semver-minor
commit and decide whether a special exception ought to be made. It is
expected that such exceptions would be evaluated, in part, on the scope
and impact of the changes on the code, the risk to ecosystem stability
incurred by accepting the change, and the expected benefit that landing the
commit will have for the ecosystem.

Any collaborator who feels a semver-minor commit should be landed in an LTS
branch should attach the `lts-agenda` label to the pull request. The LTS WG
will discuss the issue and, if necessary, will escalate the issue up to the
TSC for further discussion.

## How are LTS Branches Managed?

There are currently two LTS branches: `v6.x` and `v4.x`. Each of these is paired
with a staging branch: `v6.x-staging` and `v4.x-staging`.

As commits land on the master branch, they are cherry-picked back to each
staging branch as appropriate. If the commit applies only to the LTS branch, the
PR must be opened against the *staging* branch. Commits are selectively
pulled from the staging branch into the LTS branch only when a release is
being prepared and may be pulled into the LTS branch in a different order
than they were landed in staging.

Any collaborator may land commits into a staging branch, but only the release
team should land commits into the LTS branch while preparing a new
LTS release.

## How can I help?

When you send your pull request, consider including information about
whether your change is breaking. If you think your patch can be backported,
please feel free to include that information in the PR thread. For more
information on backporting, please see the [backporting guide][].

Several LTS related issue and PR labels have been provided:

* `lts-watch-v6.x` - tells the LTS WG that the issue/PR needs to be considered
  for landing in the `v6.x-staging` branch.
* `lts-watch-v4.x` - tells the LTS WG that the issue/PR needs to be considered
  for landing in the `v4.x-staging` branch.
* `land-on-v6.x` - tells the release team that the commit should be landed
  in a future v6.x release
* `land-on-v4.x` - tells the release team that the commit should be landed
  in a future v4.x release

Any collaborator can attach these labels to any PR/issue. As commits are
landed into the staging branches, the `lts-watch-` label will be removed.
Likewise, as commits are landed in a LTS release, the `land-on-` label will
be removed.

Collaborators are encouraged to help the LTS WG by attaching the appropriate
`lts-watch-` label to any PR that may impact an LTS release.

## How is an LTS release cut?

When the LTS working group determines that a new LTS release is required,
selected commits will be picked from the staging branch to be included in the
release. This process of making a release will be a collaboration between the
LTS working group and the Release team.

[backporting guide]: doc/guides/backporting-to-release-lines.md
[git-node]: https://github.com/nodejs/node-core-utils/blob/master/docs/git-node.md
[git-node-metadata]: https://github.com/nodejs/node-core-utils/blob/master/docs/git-node.md#git-node-metadata
[`node-core-utils`]: https://github.com/nodejs/node-core-utils
[node-core-utils-issues]: https://github.com/nodejs/node-core-utils/issues
[node-core-utils-credentials]: https://github.com/nodejs/node-core-utils#setting-up-credentials
