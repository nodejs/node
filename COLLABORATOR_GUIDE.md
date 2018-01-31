# Node.js Collaborator Guide

**Contents**

* [Issues and Pull Requests](#issues-and-pull-requests)
  - [Managing Issues and Pull Requests](#managing-issues-and-pull-requests)
  - [Welcoming First-Time Contributors](#welcoming-first-time-contributors)
  - [Closing Issues and Pull Requests](#closing-issues-and-pull-requests)
* [Accepting Modifications](#accepting-modifications)
  - [Code Reviews and Consensus Seeking](#code-reviews-and-consensus-seeking)
  - [Waiting for Approvals](#waiting-for-approvals)
  - [Testing and CI](#testing-and-ci)
    - [Useful CI Jobs](#useful-ci-jobs)
  - [Internal vs. Public API](#internal-vs-public-api)
  - [Breaking Changes](#breaking-changes)
    - [Breaking Changes and Deprecations](#breaking-changes-and-deprecations)
    - [Breaking Changes to Internal Elements](#breaking-changes-to-internal-elements)
    - [When Breaking Changes Actually Break Things](#when-breaking-changes-actually-break-things)
      - [Reverting commits](#reverting-commits)
  - [Introducing New Modules](#introducing-new-modules)
  - [Deprecations](#deprecations)
  - [Involving the TSC](#involving-the-tsc)
* [Landing Pull Requests](#landing-pull-requests)
  - [Technical HOWTO](#technical-howto)
  - [Troubleshooting](#troubleshooting)
  - [I Just Made a Mistake](#i-just-made-a-mistake)
  - [Long Term Support](#long-term-support)
    - [What is LTS?](#what-is-lts)
    - [How does LTS work?](#how-does-lts-work)
    - [Landing semver-minor commits in LTS](#landing-semver-minor-commits-in-lts)
    - [How are LTS Branches Managed?](#how-are-lts-branches-managed)
    - [How can I help?](#how-can-i-help)
    - [How is an LTS release cut?](#how-is-an-lts-release-cut)

This document contains information for Collaborators of the Node.js
project regarding managing the project's code, documentation, and issue tracker.

Collaborators should be familiar with the guidelines for new
contributors in [CONTRIBUTING.md](./CONTRIBUTING.md) and also
understand the project governance model as outlined in
[GOVERNANCE.md](./GOVERNANCE.md).

## Issues and Pull Requests

### Managing Issues and Pull Requests

Collaborators should feel free to take full responsibility for
managing issues and pull requests they feel qualified to handle, as
long as this is done while being mindful of these guidelines, the
opinions of other Collaborators and guidance of the [TSC][]. They
may also notify other qualified parties for more input on an issue
or a pull request.
[See "Who to CC in issues"](./doc/onboarding-extras.md#who-to-cc-in-issues)

### Welcoming First-Time Contributors

Courtesy should always be shown to individuals submitting issues and pull
requests to the Node.js project. Be welcoming to first-time contributors,
identified by the GitHub ![First-time contributor](./doc/first_timer_badge.png) badge.

For first-time contributors, check if the commit author is the same as the
pull request author, and ask if they have configured their git
username and email to their liking as per [this guide][git-username].
This is to make sure they would be promoted to "contributor" once
their pull request gets landed.

### Closing Issues and Pull Requests

Collaborators may close any issue or pull request they believe is
not relevant for the future of the Node.js project. Where this is
unclear, the issue should be left open for several days to allow for
additional discussion. Where this does not yield input from Node.js
Collaborators or additional evidence that the issue has relevance, the
issue may be closed. Remember that issues can always be re-opened if
necessary.

## Accepting Modifications

All modifications to the Node.js code and documentation should be
performed via GitHub pull requests, including modifications by
Collaborators and TSC members. A pull request must be reviewed, and usually
must also be tested with CI, before being landed into the codebase.

### Code Reviews and Consensus Seeking

All pull requests must be reviewed and accepted by a Collaborator with
sufficient expertise who is able to take full responsibility for the
change. In the case of pull requests proposed by an existing
Collaborator, an additional Collaborator is required for sign-off.

In some cases, it may be necessary to summon a qualified Collaborator
or a GitHub team to a pull request for review by @-mention.
[See "Who to CC in issues"](./doc/onboarding-extras.md#who-to-cc-in-issues)

If you are unsure about the modification and are not prepared to take
full responsibility for the change, defer to another Collaborator.

If any Collaborator objects to a change *without giving any additional
explanation or context*, and the objecting Collaborator fails to respond to
explicit requests for explanation or context within a reasonable period of
time, the objection may be dismissed. Note that this does not apply to
objections that are explained.

For non-breaking changes, if there is no disagreement amongst
Collaborators, a pull request may be landed given appropriate review.
Where there is discussion amongst Collaborators, consensus should be
sought if possible. The lack of consensus may indicate the need to
elevate discussion to the TSC for resolution (see below).

Breaking changes (that is, pull requests that require an increase in
the major version number, known as `semver-major` changes) must be
[elevated for review by the TSC](#involving-the-tsc).
This does not necessarily mean that the PR must be put onto the TSC meeting
agenda. If multiple TSC members approve (`LGTM`) the PR and no Collaborators
oppose the PR, it can be landed. Where there is disagreement among TSC members
or objections from one or more Collaborators, `semver-major` pull requests
should be put on the TSC meeting agenda.

#### Helpful resources

* How to respectfully and usefully review code, part [one](https://mtlynch.io/human-code-reviews-1/) and [two](https://mtlynch.io/human-code-reviews-2/)
* [How to write a positive code review](https://css-tricks.com/code-review-etiquette/)

### Waiting for Approvals

Before landing pull requests, sufficient time should be left for input
from other Collaborators. In general, leave at least 48 hours during the
week and 72 hours over weekends to account for international time
differences and work schedules. However, certain types of pull requests
can be fast-tracked and may be landed after a shorter delay. For example:

* Focused changes that affect only documentation and/or the test suite:
  * `code-and-learn` tasks typically fall into this category.
  * `good-first-issue` pull requests may also be suitable.
* Changes that fix regressions:
  * Regressions that break the workflow (red CI or broken compilation).
  * Regressions that happen right before a release, or reported soon after.

When a pull request is deemed suitable to be fast-tracked, label it with
`fast-track`. The pull request can be landed once 2 or more Collaborators
approve both the pull request and the fast-tracking request, and the necessary
CI testing is done.

### Testing and CI

All bugfixes require a test case which demonstrates the defect. The
test should *fail* before the change, and *pass* after the change.

All pull requests that modify executable code should be subjected to
continuous integration tests on the
[project CI server](https://ci.nodejs.org/).
The pull request should have a CI status indicator if possible.

#### Useful CI Jobs

* [`node-test-pull-request`](https://ci.nodejs.org/job/node-test-pull-request/)
is the standard CI run we do to check Pull Requests. It triggers `node-test-commit`,
which runs the `build-ci` and `test-ci` targets on all supported platforms.

* [`node-test-linter`](https://ci.nodejs.org/job/node-test-linter/)
only runs the linter targets, which is useful for changes that only affect comments
or documentation.

* [`node-test-pull-request-lite`](https://ci.nodejs.org/job/node-test-pull-request-lite/)
only runs the linter job, as well as the tests on LinuxONE. Should only be used for
trivial changes that do not require being tested on all platforms.

* [`citgm-smoker`](https://ci.nodejs.org/job/citgm-smoker/)
uses [`CitGM`](https://github.com/nodejs/citgm) to allow you to run `npm install && npm test`
on a large selection of common modules. This is useful to check whether a
change will cause breakage in the ecosystem. To test Node.js ABI changes
you can run [`citgm-abi-smoker`](https://ci.nodejs.org/job/citgm-abi-smoker/).

* [`node-stress-single-test`](https://ci.nodejs.org/job/node-stress-single-test/)
is designed to allow one to run a group of tests over and over on a specific
platform to confirm that the test is reliable.

* [`node-test-commit-v8-linux`](https://ci.nodejs.org/job/node-test-commit-v8-linux/)
is designed to allow validation of changes to the copy of V8 in the Node.js
tree by running the standard V8 tests. It should be run whenever the
level of V8 within Node.js is updated or new patches are floated on V8.

### Internal vs. Public API

Due to the nature of the JavaScript language, it can often be difficult to
establish a clear distinction between which parts of the Node.js implementation
represent the public API Node.js users should assume to be stable and which
are part of the internal implementation details of Node.js itself. A rule of
thumb is to base the determination off what functionality is actually
documented in the official Node.js API documentation. However, it has been
repeatedly demonstrated that either the documentation does not completely cover
implemented behavior or that Node.js users have come to rely heavily on
undocumented aspects of the Node.js implementation.

The following general rules should be followed to determine which aspects of the
Node.js API are internal:

- All functionality exposed via `process.binding(...)` is internal.
- All functionality implemented in `lib/internal/**/*.js` is internal unless it
  is re-exported by code in `lib/*.js` or documented as part of the Node.js
  Public API.
- Any object property or method whose key is a non-exported `Symbol` is an
  internal property.
- Any object property or method whose key begins with the underscore `_` prefix
  is internal unless it is documented as part of the Node.js Public API.
- Any object, property, method, argument, behavior, or event not documented in
  the Node.js documentation is internal.
- Any native C/C++ APIs/ABIs exported by the Node.js `*.h` header files that
  are hidden behind the `NODE_WANT_INTERNALS` flag are internal.

Exceptions can be made if use or behavior of a given internal API can be
demonstrated to be sufficiently relied upon by the Node.js ecosystem such that
any changes would cause too much breakage. The threshold for what qualifies as
too much breakage is to be decided on a case-by-case basis by the TSC.

If it is determined that a currently undocumented object, property, method,
argument, or event *should* be documented, then a pull request adding the
documentation is required in order for it to be considered part of the public
API.

Making a determination about whether something *should* be documented can be
difficult and will need to be handled on a case-by-case basis. For instance, if
one documented API cannot be used successfully without the use of a second
*currently undocumented* API, then the second API *should* be documented. If
using an API in a manner currently undocumented achieves a particular useful
result, a decision will need to be made whether or not that falls within the
supported scope of that API; and if it does, it should be documented.

See [Breaking Changes to Internal Elements](#breaking-changes-to-internal-elements)
on how to handle those types of changes.

### Breaking Changes

Backwards-incompatible changes may land on the master branch at any time after
sufficient review by Collaborators and approval of at least two TSC members.

Examples of breaking changes include:

* removal or redefinition of existing API arguments
* changing return values
* removing or modifying existing properties on an options argument
* adding or removing errors
* altering expected timing of an event
* changing the side effects of using a particular API

Purely additive changes (e.g. adding new events to `EventEmitter`
implementations, adding new arguments to a method in a way that allows
existing code to continue working without modification, or adding new
properties to an options argument) are semver-minor changes.

#### Breaking Changes and Deprecations

With a few exceptions outlined below, when backward-incompatible changes to a
*Public* API are necessary, the existing API *must* be deprecated *first* and
the new API either introduced in parallel or added after the next major Node.js
version following the deprecation as a replacement for the deprecated API. In
other words, as a general rule, existing *Public* APIs *must not* change (in a
backward-incompatible way) without a deprecation.

Exceptions to this rule may be made in the following cases:

* Adding or removing errors thrown or reported by a Public API;
* Changing error messages;
* Altering the timing and non-internal side effects of the Public API.

Such changes *must* be handled as semver-major changes but MAY be landed
without a [Deprecation cycle](#deprecation-cycle).

Note that errors thrown, along with behaviors and APIs implemented by
dependencies of Node.js (e.g. those originating from V8) are generally not
under the control of Node.js and therefore *are not directly subject to this
policy*. However, care should still be taken when landing updates to
dependencies when it is known or expected that breaking changes to error
handling may have been made. Additional CI testing may be required.

From time-to-time, in particularly exceptional cases, the TSC may be asked to
consider and approve additional exceptions to this rule.

For more information, see [Deprecations](#deprecations).

#### Breaking Changes to Internal Elements

Breaking changes to internal elements are permitted in semver-patch or
semver-minor commits but Collaborators should take significant care when
making and reviewing such changes. Before landing such commits, an effort
must be made to determine the potential impact of the change in the ecosystem
by analyzing current use and by validating such changes through ecosystem
testing using the [Canary in the Goldmine](https://github.com/nodejs/citgm)
tool. If a change cannot be made without ecosystem breakage, then TSC review is
required before landing the change as anything less than semver-major.

If a determination is made that a particular internal API (for instance, an
underscore `_` prefixed property) is sufficiently relied upon by the ecosystem
such that any changes may break user code, then serious consideration should be
given to providing an alternative Public API for that functionality before any
breaking changes are made.

#### When Breaking Changes Actually Break Things

Because breaking (semver-major) changes are permitted to land on the master
branch at any time, at least some subset of the user ecosystem may be adversely
affected in the short term when attempting to build and use Node.js directly
from the master branch. This potential instability is why Node.js offers
distinct Current and LTS release streams that offer explicit stability
guarantees.

Specifically:

* Breaking changes should *never* land in Current or LTS except when:
  * Resolving critical security issues.
  * Fixing a critical bug (e.g. fixing a memory leak) requires a breaking
    change.
  * There is TSC consensus that the change is required.
* If a breaking commit does accidentally land in a Current or LTS branch, an
  attempt to fix the issue will be made before the next release; If no fix is
  provided then the commit will be reverted.

When any changes are landed on the master branch and it is determined that the
changes *do* break existing code, a decision may be made to revert those
changes either temporarily or permanently. However, the decision to revert or
not can often be based on many complex factors that are not easily codified. It
is also possible that the breaking commit can be labeled retroactively as a
semver-major change that will not be backported to Current or LTS branches.

##### Reverting commits

Commits are reverted with `git revert <HASH>`, or `git revert <FROM>..<TO>` for
multiple commits. Commit metadata and the reason for the revert should be
appended. Commit message rules about line length and subsystem can be ignored.
A Pull Request should be raised and approved like any other change.

### Introducing New Modules

Semver-minor commits that introduce new core modules should be treated with
extra care.

The name of the new core module should not conflict with any existing
module in the ecosystem unless a written agreement with the owner of those
modules is reached to transfer ownership.

If the new module name is free, a Collaborator should register a placeholder
in the module registry as soon as possible, linking to the pull request that
introduces the new core module.

Pull requests introducing new core modules:

* Must be left open for at least one week for review.
* Must be labeled using the `tsc-review` label.
* Must have signoff from at least two TSC members.

New core modules must be landed with a [Stability Index][] of Experimental,
and must remain Experimental until a semver-major release.

For new modules that involve significant effort, non-trivial additions to
Node.js or significant new capabilities, an [Enhancement Proposal][] is
recommended but not required.

### Deprecations

_Deprecation_ refers to the identification of Public APIs that should no longer
be used and that may be removed or modified in backward-incompatible ways in
a future major release of Node.js. Deprecation may be used with internal APIs if
there is expected impact on the user community.

Node.js uses three Deprecation levels:

* *Documentation-Only Deprecation* refers to elements of the Public API that are
  being staged for deprecation in a future Node.js major release. An explicit
  notice indicating the deprecated status is added to the API documentation
  but no functional changes are implemented in the code. There will be no
  runtime deprecation warnings emitted for such deprecations by default.
  Documentation-only deprecations may trigger a runtime warning when launched
  with [`--pending-deprecation`][] flag (or its alternative,
  `NODE_PENDING_DEPRECATION=1` environment variable).

* *Runtime Deprecation* refers to the use of process warnings emitted at
  runtime the first time that a deprecated API is used. A command-line
  switch can be used to escalate such warnings into runtime errors that will
  cause the Node.js process to exit. As with Documentation-Only Deprecation,
  the documentation for the API must be updated to clearly indicate the
  deprecated status.

* *End-of-life* refers to APIs that have gone through Runtime Deprecation and
  are ready to be removed from Node.js entirely.

Documentation-Only Deprecations may be handled as semver-minor or semver-major
changes. Such deprecations have no impact on the successful operation of running
code and therefore should not be viewed as breaking changes.

Runtime Deprecations and End-of-life APIs (internal or public) must be
handled as semver-major changes unless there is TSC consensus to land the
deprecation as a semver-minor.

All Documentation-Only and Runtime deprecations will be assigned a unique
identifier that can be used to persistently refer to the deprecation in
documentation, emitted process warnings, or errors thrown. Documentation for
these identifiers will be included in the Node.js API documentation and will
be immutable once assigned. Even if End-of-Life code is removed from Node.js,
the documentation for the assigned deprecation identifier must remain in the
Node.js API documentation.

<a id="deprecation-cycle"></a>
A _Deprecation cycle_ is one full Node.js major release during which an API
has been in one of the three Deprecation levels. (Note that Documentation-Only
Deprecations may land in a Node.js minor release but must not be upgraded to
a Runtime Deprecation until the next major release.)

No API can be moved to End-of-life without first having gone through a
Runtime Deprecation cycle.

A best effort will be made to communicate pending deprecations and associated
mitigations with the ecosystem as soon as possible (preferably before the pull
request adding the deprecation lands on the master branch). All deprecations
included in a Node.js release should be listed prominently in the "Notable
Changes" section of the release notes.

### Involving the TSC

Collaborators may opt to elevate pull requests or issues to the [TSC][] for
discussion by assigning the `tsc-review` label or @-mentioning the
`@nodejs/tsc` GitHub team. This should be done where a pull request:

- is labeled `semver-major`, or
- has a significant impact on the codebase, or
- is inherently controversial, or
- has failed to reach consensus amongst the Collaborators who are
  actively participating in the discussion.

The TSC should serve as the final arbiter where required.

## Landing Pull Requests

* Please never use GitHub's green ["Merge Pull Request"](https://help.github.com/articles/merging-a-pull-request/#merging-a-pull-request-on-github) button.
  * If you do, please force-push removing the merge.
  * Reasons for not using the web interface button:
    * The merge method will add an unnecessary merge commit.
    * The squash & merge method has been known to add metadata to the
    commit title (the PR #).
    * If more than one author has contributed to the PR, keep the most recent
      author when squashing.

Review the commit message to ensure that it adheres to the guidelines outlined
in the [contributing](./doc/guides/contributing/pull-requests.md#commit-message-guidelines) guide.

Add all necessary [metadata](#metadata) to commit messages before landing.

See the commit log for examples such as
[this one](https://github.com/nodejs/node/commit/b636ba8186) if unsure
exactly how to format your commit messages.

Additionally:
- Double check PRs to make sure the person's _full name_ and email
  address are correct before merging.
- All commits should be self-contained (meaning every commit should pass all
  tests). This makes it much easier when bisecting to find a breaking change.

### Technical HOWTO

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
[CONTRIBUTING.md](./doc/guides/contributing/pull-requests.md#step-1-fork)):

```text
$ git fetch upstream
$ git merge --ff-only upstream/master
```

Apply external patches:

```text
$ curl -L https://github.com/nodejs/node/pull/xxx.patch | git am --whitespace=fix
```

If the merge fails even though recent CI runs were successful, then a 3-way merge may
be required.  In this case try:

```text
$ git am --abort
$ curl -L https://github.com/nodejs/node/pull/xxx.patch | git am -3 --whitespace=fix
```
If the 3-way merge succeeds you can proceed, but make sure to check the changes
against the original PR carefully and build/test on at least one platform
before landing. If the 3-way merge fails, then it is most likely that a conflicting
PR has landed since the CI run and you will have to ask the author to rebase.

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
[commit message guidelines](./doc/guides/contributing/pull-requests.md#commit-message-guidelines).

<a name="metadata"></a>
* Modify the original commit message to include additional metadata regarding
  the change process. ([`node-core-utils`][] fetches the metadata for you.)

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

### Troubleshooting

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

### I Just Made a Mistake

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

### Long Term Support

#### What is LTS?

Long Term Support (often referred to as *LTS*) guarantees application developers
a 30-month support cycle with specific versions of Node.js.

You can find more information
[in the full release plan](https://github.com/nodejs/Release#release-plan).

#### How does LTS work?

Once a Current branch enters LTS, changes in that branch are limited to bug
fixes, security updates, possible npm updates, documentation updates, and
certain performance improvements that can be demonstrated to not break existing
applications. Semver-minor changes are only permitted if required for bug fixes
and then only on a case-by-case basis with LTS WG and possibly Technical
Steering Committee (TSC) review. Semver-major changes are permitted only if
required for security-related fixes.

Once a Current branch moves into Maintenance mode, only **critical** bugs,
**critical** security fixes, and documentation updates will be permitted.

#### Landing semver-minor commits in LTS

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

#### How are LTS Branches Managed?

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

#### How can I help?

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

#### How is an LTS release cut?

When the LTS working group determines that a new LTS release is required,
selected commits will be picked from the staging branch to be included in the
release. This process of making a release will be a collaboration between the
LTS working group and the Release team.

[backporting guide]: doc/guides/backporting-to-release-lines.md
[Stability Index]: doc/api/documentation.md#stability-index
[Enhancement Proposal]: https://github.com/nodejs/node-eps
[`--pending-deprecation`]: doc/api/cli.md#--pending-deprecation
[git-username]: https://help.github.com/articles/setting-your-username-in-git/
[`node-core-utils`]: https://github.com/nodejs/node-core-utils
[TSC]: https://github.com/nodejs/TSC
