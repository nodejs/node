# Node.js Collaborator Guide

**Contents**

* [Issues and Pull Requests](#issues-and-pull-requests)
* [Accepting Modifications](#accepting-modifications)
 - [Involving the TC](#involving-the-tc)
* [Landing Pull Requests](#landing-pull-requests)
 - [Technical HOWTO](#technical-howto)
 - [I Just Made a Mistake](#i-just-made-a-mistake)

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
opinions of other Collaborators and guidance of the TC.

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
Collaborators and TC members.

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

Where there is no disagreement amongst Collaborators, a pull request
may be landed given appropriate review. Where there is discussion
amongst Collaborators, consensus should be sought if possible. The
lack of consensus may indicate the need to elevate discussion to the
TC for resolution (see below).

All bugfixes require a test case which demonstrates the defect. The
test should *fail* before the change, and *pass* after the change.

All pull requests that modify executable code should be subjected to
continuous integration tests on the
[project CI server](https://ci.nodejs.org/).

### Involving the TC

Collaborators may opt to elevate pull requests or issues to the TC for
discussion by assigning the ***tc-agenda*** tag. This should be done
where a pull request:

- has a significant impact on the codebase,
- is inherently controversial; or
- has failed to reach consensus amongst the Collaborators who are
  actively participating in the discussion.

The TC should serve as the final arbiter where required.

## Landing Pull Requests

Always modify the original commit message to include additional meta
information regarding the change process:

- A `Reviewed-By: Name <email>` line for yourself and any
  other Collaborators who have reviewed the change.
- A `PR-URL:` line that references the full GitHub URL of the original
  pull request being merged so it's easy to trace a commit back to the
  conversation that led up to that change.
- A `Fixes: X` line, where _X_ is either includes the full GitHub URL
  for an issue, and/or the hash and commit message if the commit fixes
  a bug in a previous commit. Multiple `Fixes:` lines may be added if
  appropriate.

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
