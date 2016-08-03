# Node.js Collaborator Guide

**Contents**

* [Issues and Pull Requests](#issues-and-pull-requests)
* [Accepting Modifications](#accepting-modifications)
 - [Involving the CTC](#involving-the-ctc)
* [Landing Pull Requests](#landing-pull-requests)
 - [Technical HOWTO](#technical-howto)
 - [I Just Made a Mistake](#i-just-made-a-mistake)
 - [Long Term Support](#long-term-support)

This document contains information for Collaborators of the Node.js
project regarding maintaining the code, documentation and issues.

Collaborators should be familiar with the guidelines for new
contributors in [CONTRIBUTING.md](./CONTRIBUTING.md) and also
understand the project governance model as outlined in
[GOVERNANCE.md](./GOVERNANCE.md).

## Issues and Pull Requests

Courtesy should always be shown to individuals submitting issues and
pull requests to the Node.js project.

Collaborators should feel free to take full responsibility for
managing issues and pull requests they feel qualified to handle, as
long as this is done while being mindful of these guidelines, the
opinions of other Collaborators and guidance of the CTC.

Collaborators may **close** any issue or pull request they believe is
not relevant for the future of the Node.js project. Where this is
unclear, the issue should be left open for several days to allow for
additional discussion. Where this does not yield input from Node.js
Collaborators or additional evidence that the issue has relevance, the
issue may be closed. Remember that issues can always be re-opened if
necessary.

## Accepting Modifications

All modifications to the Node.js code and documentation should be
performed via GitHub pull requests, including modifications by
Collaborators and CTC members.

All pull requests must be reviewed and accepted by a Collaborator with
sufficient expertise who is able to take full responsibility for the
change. In the case of pull requests proposed by an existing
Collaborator, an additional Collaborator is required for sign-off.

In some cases, it may be necessary to summon a qualified Collaborator
to a pull request for review by @-mention.

If you are unsure about the modification and are not prepared to take
full responsibility for the change, defer to another Collaborator.

Before landing pull requests, sufficient time should be left for input
from other Collaborators. Leave at least 48 hours during the week and
72 hours over weekends to account for international time differences
and work schedules. Trivial changes (e.g. those which fix minor bugs
or improve performance without affecting API or causing other
wide-reaching impact) may be landed after a shorter delay.

For non-breaking changes, if there is no disagreement amongst Collaborators, a
pull request may be landed given appropriate review. Where there is discussion
amongst Collaborators, consensus should be sought if possible. The
lack of consensus may indicate the need to elevate discussion to the
CTC for resolution (see below).

Breaking changes (that is, pull requests that require an increase in the
major version number, known as `semver-major` changes) must be elevated for
review by the CTC. This does not necessarily mean that the PR must be put onto
the CTC meeting agenda. If multiple CTC members approve (`LGTM`) the PR and no
Collaborators oppose the PR, it can be landed. Where there is disagreement among
CTC members or objections from one or more Collaborators, `semver-major` pull
requests should be put on the CTC meeting agenda.

All bugfixes require a test case which demonstrates the defect. The
test should *fail* before the change, and *pass* after the change.

All pull requests that modify executable code should be subjected to
continuous integration tests on the
[project CI server](https://ci.nodejs.org/).

### Involving the CTC

Collaborators may opt to elevate pull requests or issues to the CTC for
discussion by assigning the ***ctc-agenda*** tag. This should be done
where a pull request:

- has a significant impact on the codebase,
- is inherently controversial; or
- has failed to reach consensus amongst the Collaborators who are
  actively participating in the discussion.

The CTC should serve as the final arbiter where required.

## Landing Pull Requests

Always modify the original commit message to include additional meta
information regarding the change process:

- A `Reviewed-By: Name <email>` line for yourself and any
  other Collaborators who have reviewed the change.
- A `PR-URL:` line that references the *full* GitHub URL of the original
  pull request being merged so it's easy to trace a commit back to the
  conversation that led up to that change.
- A `Fixes: X` line, where _X_ either includes the *full* GitHub URL
  for an issue, and/or the hash and commit message if the commit fixes
  a bug in a previous commit. Multiple `Fixes:` lines may be added if
  appropriate.

Review the commit message to ensure that it adheres to the guidelines
outlined in the [contributing](https://github.com/nodejs/node/blob/master/CONTRIBUTING.md#step-3-commit) guide.

See the commit log for examples such as
[this one](https://github.com/nodejs/node/commit/b636ba8186) if unsure
exactly how to format your commit messages.

Additionally:

- Double check PRs to make sure the person's _full name_ and email
  address are correct before merging.
- Except when updating dependencies, all commits should be self
  contained (meaning every commit should pass all tests). This makes
  it much easier when bisecting to find a breaking change.

### Technical HOWTO

_Optional:_ ensure that you are not in a borked `am`/`rebase` state

```text
$ git am --abort
$ git rebase --abort
```

Checkout proper target branch

```text
$ git checkout master
```

Update the tree

```text
$ git fetch origin
$ git merge --ff-only origin/master
```

Apply external patches

```text
$ curl -L https://github.com/nodejs/node/pull/xxx.patch | git am --whitespace=fix
```

Check and re-review the changes

```text
$ git diff origin/master
```

Check number of commits and commit messages

```text
$ git log origin/master...master
```

If there are multiple commits that relate to the same feature or
one with a feature and separate with a test for that feature,
you'll need to use `squash` or `fixup`:

```text
$ git rebase -i origin/master
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

Time to push it:

```text
$ git push origin master
```

### I Just Made a Mistake

With `git`, there's a way to override remote trees by force pushing
(`git push -f`). This should generally be seen as forbidden (since
you're rewriting history on a repository other people are working
against) but is allowed for simpler slip-ups such as typos in commit
messages. However, you are only allowed to force push to any Node.js
branch within 10 minutes from your original push. If someone else
pushes to the branch or the 10 minute period passes, consider the
commit final.

### Long Term Support

#### What is LTS?

Long Term Support (often referred to as *LTS*) guarantees application developers
a 30 month support cycle with specific versions of Node.js.

You can find more information [in the full LTS plan](https://github.com/nodejs/lts#lts-plan).

#### How does LTS work?

Once a stable branch enters LTS, changes in that branch are limited to bug
fixes, security updates, possible npm updates, documentation updates, and
certain performance improvements that can be demonstrated to not break existing
applications. Semver-minor changes are only permitted if required for bug fixes
and then only on a case-by-case basis with LTS WG and possibly Core Technical
Committee (CTC) review. Semver-major changes are permitted only if required for
security related fixes.

Once a stable branch moves into Maintenance mode, only **critical** bugs,
**critical** security fixes, and documentation updates will be permitted.

#### Landing semver-minor commits in LTS

The default policy is to not land semver-minor or higher commits in any LTS
branch. However, the LTS WG or CTC can evaluate any individual semver-minor
commit and decide whether a special exception ought to be made. It is
expected that such exceptions would be evaluated, in part, on the scope
and impact of the changes on the code, the risk to ecosystem stability
incurred by accepting the change, and the expected benefit that landing the
commit will have for the ecosystem.

Any collaborator who feels a semver-minor commit should be landed in an LTS
branch should attach the `lts-agenda` label to the pull request. The LTS WG
will discuss the issue and, if necessary, will escalate the issue up to the
CTC for further discussion.

#### How are LTS Branches Managed?

There are currently three LTS branches: `v4.x`, `v0.10`, and `v0.12`. Each
of these is paired with a "staging" branch: `v4.x-staging`, `v0.10-staging`,
and `v0.12-staging`.

As commits land in `master`, they are cherry-picked back to each staging
branch as appropriate. If the commit applies only to the LTS branch, the
PR must be opened against the *staging* branch. Commits are selectively
pulled from the staging branch into the LTS branch only when a release is
being prepared and may be pulled into the LTS branch in a different order
than they were landed in staging.

Any collaborator may land commits into a staging branch, but only the release
team should land commits into the LTS branch while preparing a new
LTS release.

#### How can I help?

When you send your pull request, consider including information about
whether your change is breaking. If you think your patch can be backported,
please feel free to include that information in the PR thread.

Several LTS related issue and PR labels have been provided:

* `lts-watch-v4.x` - tells the LTS WG that the issue/PR needs to be considered
  for landing in the `v4.x-staging` branch.
* `lts-watch-v0.10` - tells the LTS WG that the issue/PR needs to be considered
  for landing in the `v0.10-staging` branch.
* `lts-watch-v0.12` - tells the LTS WG that the issue/PR needs to be considered
  for landing in the `v0.12-staging` branch.
* `land-on-v4.x` - tells the release team that the commit should be landed
  in a future v4.x release
* `land-on-v0.10` - tells the release team that the commit should be landed
  in a future v0.10 release
* `land-on-v0.12` - tells the release team that the commit should be landed
  in a future v0.12 release

Any collaborator can attach these labels to any PR/issue. As commits are
landed into the staging branches, the `lts-watch-` label will be removed.
Likewise, as commits are landed in a LTS release, the `land-on-` label will
be removed.

Collaborators are encouraged to help the LTS WG by attaching the appropriate
`lts-watch-` label to any PR that may impact an LTS release.

#### How is an LTS release cut?

When the LTS working group determines that a new LTS release is required,
selected commits will be picked from the staging branch to be included in the
release. This process of making a release will be a collaboration between the
LTS working group and the Release team.
