# Node.js Collaborator Guide

**Contents**

* [Issues and Pull Requests](#issues-and-pull-requests)
* [Accepting Modifications](#accepting-modifications)
 - [Internal vs. Public API](#internal-vs-public-api)
 - [Breaking Changes](#breaking-changes)
 - [Deprecations](#deprecations)
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

[**See "Who to CC in issues"**](./doc/onboarding-extras.md#who-to-cc-in-issues)

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

For non-breaking changes, if there is no disagreement amongst
Collaborators, a pull request may be landed given appropriate review.
Where there is discussion amongst Collaborators, consensus should be
sought if possible. The lack of consensus may indicate the need to
elevate discussion to the CTC for resolution (see below).

Breaking changes (that is, pull requests that require an increase in
the major version number, known as `semver-major` changes) must be
elevated for review by the CTC. This does not necessarily mean that the
PR must be put onto the CTC meeting agenda. If multiple CTC members
approve (`LGTM`) the PR and no Collaborators oppose the PR, it can be
landed. Where there is disagreement among CTC members or objections
from one or more Collaborators, `semver-major` pull requests should be
put on the CTC meeting agenda.

All bugfixes require a test case which demonstrates the defect. The
test should *fail* before the change, and *pass* after the change.

All pull requests that modify executable code should be subjected to
continuous integration tests on the
[project CI server](https://ci.nodejs.org/).

### Internal vs. Public API

Due to the nature of the JavaScript language, it can often be difficult to
establish a clear distinction between which parts of the Node.js implementation
represent the "public" API Node.js users should assume to be stable and which
are considered part of the "internal" implementation detail of Node.js itself.
A general rule of thumb has been to base the determination off what
functionality is actually *documented* in the official Node.js API
documentation. However, it has been repeatedly demonstrated that either the
documentation does not completely cover implemented behavior or that Node.js
users have come to rely heavily on undocumented aspects of the Node.js
implementation.

While there are numerous exceptions, the following general rules should be
followed to determine which aspects of the Node.js API are considered
"internal":

- Any and all functionality exposed via `process.binding(...)` is considered to
  be internal and *not* part of the Node.js Public API.
- Any and all functionality implemented in `lib/internal/**/*.js` that is not
  re-exported by code in `lib/*.js`, or is not documented as part of the
  Node.js Public API, is considered to be internal.
- Any object property or method whose key is a non-exported `Symbol` is
  considered to be an internal property.
- Any object property or method whose key begins with the underscore `_` prefix,
  and is not documented as part of the Node.js Public API, is considered to be
  an internal property.
- Any object, property, method, argument, behavior, or event not documented in
  the Node.js documentation is considered to be internal.
- Any native C/C++ APIs/ABIs exported by the Node.js `*.h` header files that
  are hidden behind the `NODE_WANT_INTERNALS` flag are considered to be
  internal.

Exception to each of these points can be made if use or behavior of a given
internal API can be demonstrated to be sufficiently relied upon by the Node.js
ecosystem such that any changes would cause too much breakage. The threshold
for what qualifies as "too much breakage" is to be decided on a case-by-case
basis by the CTC.

If it is determined that a currently undocumented object, property, method,
argument, or event *should* be documented, then a pull request adding the
documentation is required in order for it to be considered part of the "public"
API.

Making a determination about whether something *should* be documented can be
difficult and will need to be handled on a case-by-case basis. For instance, if
one documented API cannot be used successfully without the use of a second
*currently undocumented* API, then the second API *should* be documented. If
using an API in a manner currently undocumented achieves a particular useful
result, a decision will need to be made whether or not that falls within the
supported scope of that API; and if it does, it should be documented.

Breaking changes to internal elements are permitted in semver-patch or
semver-minor commits but Collaborators should take significant care when
making and reviewing such changes. Before landing such commits, an effort
must be made to determine the potential impact of the change in the ecosystem
by analyzing current use and by validating such changes through ecosystem
testing using the [Canary in the Goldmine](https://github.com/nodejs/citgm)
tool. If a change cannot be made without ecosystem breakage, then CTC review is
required before landing the change as anything less than semver-major.

If a determination is made that a particular internal API (for instance, an
underscore `_` prefixed property) is sufficiently relied upon by the ecosystem
such that any changes may break user code, then serious consideration should be
given to providing an alternative Public API for that functionality before any
breaking changes are made.

### Breaking Changes

Backwards-incompatible changes may land on the master branch at any time after
sufficient review by collaborators and approval of at least two CTC members.

Examples of breaking changes include, but are not necessarily limited to,
removal or redefinition of existing API arguments, changing return values
(except when return values do not currently exist), removing or modifying
existing properties on an options argument, adding or removing errors,
changing error messages in any way, altering expected timing of an event (e.g.
moving from sync to async responses or vice versa), and changing the
non-internal side effects of using a particular API.

With a few notable exceptions outlined below, when backwards incompatible
changes to a *Public* API are necessary, the existing API *must* be deprecated
*first* and the new API either introduced in parallel or added after the next
major Node.js version following the deprecation as a replacement for the
deprecated API. In other words, as a general rule, existing *Public* APIs
*must not* change (in a backwards incompatible way) without a deprecation.

Exception to this rule is given in the following cases:

* Adding or removing errors thrown or reported by a Public API;
* Changing error messages;
* Altering the timing and non-internal side effects of the Public API.

Such changes *must* be handled as semver-major changes but MAY be landed
without a [Deprecation cycle](#deprecation-cycle).

From time-to-time, in particularly exceptional cases, the CTC may be asked to
consider and approve additional exceptions to this rule.

Purely additive changes (e.g. adding new events to EventEmitter
implementations, adding new arguments to a method in a way that allows
existing code to continue working without modification, or adding new
properties to an options argument) are handled as semver-minor changes.

Note that errors thrown, along with behaviors and APIs implemented by
dependencies of Node.js (e.g. those originating from V8) are generally not
under the control of Node.js and therefore *are not directly subject to this
policy*. However, care should still be taken when landing updates to
dependencies when it is known or expected that breaking changes to error
handling may have been made. Additional CI testing may be required.

#### When breaking changes actually break things

Breaking changes are difficult primarily because they change the fundamental
assumptions a user of Node.js has when writing their code and can cause
existing code to stop functioning as expected -- costing developers and users
time and energy to fix.

Because breaking (semver-major) changes are permitted to land in master at any
time, it should be *understood and expected* that at least some subset of the
user ecosystem *may* be adversely affected *in the short term* when attempting
to build and use Node.js directly from master. This potential instability is
precisely why Node.js offers distinct Current and LTS release streams that
offer explicit stability guarantees.

Specifically:

* Breaking changes should *never* land in Current or LTS except when:
  * Resolving critical security issues.
  * Fixing a critical bug (e.g. fixing a memory leak) requires a breaking
    change.
  * There is CTC consensus that the change is required.
* If a breaking commit does accidentally land in a Current or LTS branch, an
  attempt to fix the issue will be made before the next release; If no fix is
  provided then the commit will be reverted.

When any change is landed in master, and it is determined that the such
changes *do* break existing code, a decision may be made to revert those
changes either temporarily or permanently. However, the decision to revert or
not can often be based on many complex factors that are not easily codified. It
is also possible that the breaking commit can be labeled retroactively as a
semver-major change that will not be backported to Current or LTS branches.

### Deprecations

Deprecation refers to the identification of Public APIs that should no longer
be used and that may be removed or modified in non-backwards compatible ways in
a future major release of Node.js. Deprecation *may* be used with internal APIs
if there is expected impact on the user community.

Node.js uses three fundamental Deprecation levels:

* *Documentation-Only Deprecation* refers to elements of the Public API that are
  being staged for deprecation in a future Node.js major release. An explicit
  notice indicating the deprecated status is added to the API documentation
  *but no functional changes are implemented in the code*. There will be no
  runtime deprecation warning emitted for such deprecations.

* *Runtime Deprecation* refers to the use of process warnings emitted at
  runtime the first time that a deprecated API is used. A command-line
  switch can be used to escalate such warnings into runtime errors that will
  cause the Node.js process to exit. As with Documentation-Only Deprecation,
  the documentation for the API must be updated to clearly indicate the
  deprecated status.

* *End-of-life* refers to APIs that have gone through Runtime Deprecation and
  are ready to be removed from Node.js entirely.

Documentation-Only Deprecations *may* be handled as semver-minor or
semver-major changes. Such deprecations have no impact on the successful
operation of running code and therefore should not be viewed as breaking
changes.

Runtime Deprecations and End-of-life APIs (internal or public) *must* be
handled as semver-major changes unless there is CTC consensus to land the
deprecation as a semver-minor.

All Documentation-Only and Runtime deprecations will be assigned a unique
identifier that can be used to persistently refer to the deprecation in
documentation, emitted process warnings, or errors thrown. Documentation for
these identifiers will be included in the Node.js API documentation and will
be immutable once assigned. Even if End-of-Life code is removed from Node.js,
the documentation for the assigned deprecation identifier must remain in the
Node.js API documentation.

<a id="deprecation-cycle"></a>
A "Deprecation cycle" is one full Node.js major release during which an API
has been in one of the three Deprecation levels. (Note that Documentation-Only
Deprecations may land in a Node.js minor release but must not be upgraded to
a Runtime Deprecation until the next major release.)

No API can be moved to End-of-life without first having gone through a
Runtime Deprecation cycle.

A best effort will be made to communicate pending deprecations and associated
mitigations with the ecosystem as soon as possible (preferably *before* the pull
request adding the deprecation lands in master). All deprecations included in
a Node.js release should be listed prominently in the "Notable Changes" section
of the release notes.

### Involving the CTC

Collaborators may opt to elevate pull requests or issues to the CTC for
discussion by assigning the `ctc-review` label. This should be done
where a pull request:

- has a significant impact on the codebase,
- is inherently controversial; or
- has failed to reach consensus amongst the Collaborators who are
  actively participating in the discussion.

The CTC should serve as the final arbiter where required.

## Landing Pull Requests

* Please never use GitHub's green ["Merge Pull Request"](https://help.github.com/articles/merging-a-pull-request/#merging-a-pull-request-using-the-github-web-interface) button.
  * If you do, please force-push removing the merge.
  * Reasons for not using the web interface button:
    * The merge method will add an unnecessary merge commit.
    * The rebase & merge method adds metadata to the commit title.
    * The rebase method changes the author.
    * The squash & merge method has been known to add metadata to the
    commit title.
    * If more than one author has contributed to the PR, only the
    latest author will be considered during the squashing.

Always modify the original commit message to include additional meta
information regarding the change process:

- A `PR-URL:` line that references the *full* GitHub URL of the original
  pull request being merged so it's easy to trace a commit back to the
  conversation that led up to that change.
- A `Fixes: X` line, where _X_ either includes the *full* GitHub URL
  for an issue, and/or the hash and commit message if the commit fixes
  a bug in a previous commit. Multiple `Fixes:` lines may be added if
  appropriate.
- A `Refs:` line referencing a URL for any relevant background.
- A `Reviewed-By: Name <email>` line for yourself and any
  other Collaborators who have reviewed the change.
  - Useful for @mentions / contact list if something goes wrong in the PR.
  - Protects against the assumption that GitHub will be around forever.

Review the commit message to ensure that it adheres to the guidelines
outlined in the [contributing](./CONTRIBUTING.md#step-3-commit) guide.

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
* The commit message text must conform to the
[commit message guidelines](./CONTRIBUTING.md#step-3-commit).

Time to push it:

```text
$ git push origin master
```
* Optional: Force push the amended commit to the branch you used to
open the pull request. If your branch is called `bugfix`, then the
command would be `git push --force-with-lease origin master:bugfix`.
When the pull request is closed, this will cause the pull request to
show the purple merged status rather than the red closed status that is
usually used for pull requests that weren't merged. Only do this when
landing your own contributions.

* Close the pull request with a "Landed in `<commit hash>`" comment. If
your pull request shows the purple merged status then you should still
add the "Landed in <commit hash>..<commit hash>" comment if you added
multiple commits.

* `./configure && make -j8 test`
  * `-j8` builds node in parallel with 8 threads. Adjust to the number
  of cores or processor-level threads your processor has (or slightly
  more) for best results.

### I Just Made a Mistake

* Ping a CTC member.
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
a 30 month support cycle with specific versions of Node.js.

You can find more information [in the full LTS plan](https://github.com/nodejs/lts#lts-plan).

#### How does LTS work?

Once a Current branch enters LTS, changes in that branch are limited to bug
fixes, security updates, possible npm updates, documentation updates, and
certain performance improvements that can be demonstrated to not break existing
applications. Semver-minor changes are only permitted if required for bug fixes
and then only on a case-by-case basis with LTS WG and possibly Core Technical
Committee (CTC) review. Semver-major changes are permitted only if required for
security related fixes.

Once a Current branch moves into Maintenance mode, only **critical** bugs,
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

There are currently two LTS branches: `v6.x` and `v4.x`. Each of these is paired
with a "staging" branch: `v6.x-staging` and `v4.x-staging`.

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
