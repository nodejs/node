# Node.js Collaborator Guide

## Contents

* [Issues and Pull Requests](#issues-and-pull-requests)
  - [Welcoming First-Time Contributors](#welcoming-first-time-contributors)
  - [Closing Issues and Pull Requests](#closing-issues-and-pull-requests)
  - [Author ready pull requests](#author-ready-pull-requests)
  - [Handling own pull requests](#handling-own-pull-requests)
* [Accepting Modifications](#accepting-modifications)
  - [Code Reviews](#code-reviews)
  - [Consensus Seeking](#consensus-seeking)
  - [Waiting for Approvals](#waiting-for-approvals)
  - [Testing and CI](#testing-and-ci)
    - [Useful CI Jobs](#useful-ci-jobs)
  - [Internal vs. Public API](#internal-vs-public-api)
  - [Breaking Changes](#breaking-changes)
    - [Breaking Changes and Deprecations](#breaking-changes-and-deprecations)
    - [Breaking Changes to Internal Elements](#breaking-changes-to-internal-elements)
    - [Unintended Breaking Changes](#unintended-breaking-changes)
      - [Reverting commits](#reverting-commits)
  - [Introducing New Modules](#introducing-new-modules)
  - [Additions to N-API](#additions-to-n-api)
  - [Deprecations](#deprecations)
  - [Involving the TSC](#involving-the-tsc)
* [Landing Pull Requests](#landing-pull-requests)
  - [Using `git-node`](#using-git-node)
  - [Technical HOWTO](#technical-howto)
  - [Troubleshooting](#troubleshooting)
  - [I Made a Mistake](#i-made-a-mistake)
  - [Long Term Support](#long-term-support)
    - [What is LTS?](#what-is-lts)
    - [How does LTS work?](#how-does-lts-work)
    - [Landing semver-minor commits in LTS](#landing-semver-minor-commits-in-lts)
    - [How are LTS Branches Managed?](#how-are-lts-branches-managed)
    - [How can I help?](#how-can-i-help)
    - [How is an LTS release cut?](#how-is-an-lts-release-cut)
* [Who to CC in the issue tracker](#who-to-cc-in-the-issue-tracker)

This document explains how Collaborators manage the Node.js project.
Collaborators should understand the
[guidelines for new contributors](CONTRIBUTING.md) and the
[project governance model](GOVERNANCE.md).

## Issues and Pull Requests

Mind these guidelines, the opinions of other Collaborators, and guidance of the
[TSC][]. Notify other qualified parties for more input on an issue or a pull
request. See [Who to CC in the issue tracker](#who-to-cc-in-the-issue-tracker).

### Welcoming First-Time Contributors

Always show courtesy to individuals submitting issues and pull requests. Be
welcoming to first-time contributors, identified by the GitHub
![First-time contributor](./doc/first_timer_badge.png) badge.

For first-time contributors, check if the commit author is the same as the pull
request author. This way, once their pull request lands, GitHub will show them
as a _Contributor_. Ask if they have configured their git
[username][git-username] and [email][git-email] to their liking.

### Closing Issues and Pull Requests

Collaborators may close any issue or pull request that is not relevant to the
future of the Node.js project. Where this is unclear, leave the issue or pull
request open for several days to allow for discussion. Where this does not yield
evidence that the issue or pull request has relevance, close it. Remember that
issues and pull requests can always be re-opened if necessary.

### Author ready pull requests

A pull request is _author ready_ when:

* There is a CI run in progress or completed.
* There is at least one Collaborator approval.
* There are no outstanding review comments.

Please always add the `author ready` label to the pull request in that case.
Please always remove it again as soon as the conditions are not met anymore.

### Handling own pull requests

When you open a pull request, [start a CI](#testing-and-ci) right away and post
the link to it in a comment in the pull request. Later, after new code changes
or rebasing, start a new CI.

As soon as the pull request is ready to land, please do so. This allows other
Collaborators to focus on other pull requests. If your pull request is not ready
to land but is [author ready](#author-ready-pull-requests), add the
`author ready` label. If you wish to land the pull request yourself, use the
"assign yourself" link to self-assign it.

## Accepting Modifications

Contributors propose modifications to Node.js using GitHub pull requests. This
includes modifications proposed by TSC members and other Collaborators. A pull
request must pass code review and CI before landing into the codebase.

### Code Reviews

At least two Collaborators must approve a pull request before the pull request
lands. One Collaborator approval is enough if the pull request has been open
for more than seven days.

Approving a pull request indicates that the Collaborator accepts responsibility
for the change.

Approval must be from Collaborators who are not authors of the change.

In some cases, it may be necessary to summon a GitHub team to a pull request for
review by @-mention.
See [Who to CC in the issue tracker](#who-to-cc-in-the-issue-tracker).

If you are the first Collaborator to approve a pull request that has no CI yet,
please [start one](#testing-and-ci). Post the link to the CI in the PR. Please
also start a new CI if the PR creator pushed new code since the last CI run.

### Consensus Seeking

If there are no objecting Collaborators, a pull request may land if it has the
needed [approvals](#code-reviews), [CI](#testing-and-ci), and
[wait time](#waiting-for-approvals). If a pull request meets all requirements
except the [wait time](#waiting-for-approvals), please add the
[`author ready`](#author-ready-pull-requests) label.

Where there is disagreement among Collaborators, consensus should be sought if
possible. If reaching consensus is not possible, a Collaborator may escalate the
issue to the TSC.

Collaborators should not block a pull request without providing a reason.
Another Collaborator may ask an objecting Collaborator to explain their
objection. If the objector is unresponsive, another Collaborator may dismiss the
objection.

[Breaking changes](#breaking-changes) must receive
[TSC review](#involving-the-tsc). If two TSC members approve the pull request
and no Collaborators object, then it may land. If there are objections, a
Collaborator may apply the `tsc-agenda` label. That will put the pull request on
the TSC meeting agenda.

#### Helpful resources

* [How to Do Code Reviews Like a Human (Part One)](https://mtlynch.io/human-code-reviews-1/)
* [How to Do Code Reviews Like a Human (Part Two)](https://mtlynch.io/human-code-reviews-2/)
* [Code Review Etiquette](https://css-tricks.com/code-review-etiquette/)

### Waiting for Approvals

Before landing pull requests, allow 48 hours for input from other Collaborators.
Certain types of pull requests can be fast-tracked and may land after a shorter
delay. For example:

* Focused changes that affect only documentation and/or the test suite:
  * `code-and-learn` tasks often fall into this category.
  * `good-first-issue` pull requests may also be suitable.
* Changes that fix regressions:
  * Regressions that break the workflow (red CI or broken compilation).
  * Regressions that happen right before a release, or reported soon after.

To propose fast-tracking a pull request, apply the `fast-track` label. Then add
a comment that Collaborators may upvote.

If someone disagrees with the fast-tracking request, remove the label. Do not
fast-track the pull request in that case.

The pull request may be fast-tracked if two Collaborators approve the
fast-tracking request. To land, the pull request itself still needs two
Collaborator approvals and a passing CI.

Collaborators may request fast-tracking of pull requests they did not author.
In that case only, the request itself is also one fast-track approval. Upvote
the comment anyway to avoid any doubt.

### Testing and CI

All fixes must have a test case which demonstrates the defect. The test should
fail before the change, and pass after the change.

All pull requests must pass continuous integration tests on the
[project CI server](https://ci.nodejs.org/).

Do not land any pull requests without passing (green or yellow) CI runs. If
there are CI failures unrelated to the change in the pull request, try "Resume
Build". It is in the left navigation of the relevant `node-test-pull-request`
job. It will preserve all the green results from the current job but re-run
everything else.

#### Useful CI Jobs

* [`node-test-pull-request`](https://ci.nodejs.org/job/node-test-pull-request/)
is the CI job to test pull requests. It runs the `build-ci` and `test-ci`
targets on all supported platforms.

* [`node-test-pull-request-lite-pipeline`](https://ci.nodejs.org/job/node-test-pull-request-lite-pipeline/)
runs the linter job. It also runs the tests on a very fast host. This is useful
for changes that only affect comments or documentation.

* [`citgm-smoker`](https://ci.nodejs.org/job/citgm-smoker/)
uses [`CitGM`](https://github.com/nodejs/citgm) to allow you to run
`npm install && npm test` on a large selection of common modules. This is
useful to check whether a change will cause breakage in the ecosystem. To test
Node.js ABI changes you can run [`citgm-abi-smoker`](https://ci.nodejs.org/job/citgm-abi-smoker/).

* [`node-stress-single-test`](https://ci.nodejs.org/job/node-stress-single-test/)
can run a group of tests over and over on a specific platform. Use it to check
that the tests are reliable.

* [`node-test-commit-v8-linux`](https://ci.nodejs.org/job/node-test-commit-v8-linux/)
runs the standard V8 tests. Run it when updating V8 in Node.js or floating new
patches on V8.

* [`node-test-commit-custom-suites`](https://ci.nodejs.org/job/node-test-commit-custom-suites/)
enables customization of test suites and parameters. It can execute test suites
not used in other CI test runs (such as tests in the `internet` or `pummel`
directories). It can also make sure tests pass when provided with a flag not
used in other CI test runs (such as `--worker`).

### Internal vs. Public API

All functionality in the official Node.js documentation is part of the public
API. Any undocumented object, property, method, argument, behavior, or event is
internal. There are exceptions to this rule. Node.js users have come to rely on
some undocumented behaviors. Collaborators treat many of those undocumented
behaviors as public.

All undocumented functionality exposed via  `process.binding(...)` is internal.

All undocumented functionality in `lib/internal/**/*.js` is internal. It is
public, though, if it is re-exported by code in `lib/*.js`.

Non-exported `Symbol` properties and methods are internal.

Any undocumented object property or method that begins with `_` is internal.

Any native C/C++ APIs/ABIs requiring the `NODE_WANT_INTERNALS` flag are
internal.

Sometimes, there is disagreement about whether functionality is internal or
public. In those cases, the TSC makes a determination.

For undocumented APIs that are public, open a pull request documenting the API.

### Breaking Changes

At least two TSC members must approve backward-incompatible changes to the
master branch.

Examples of breaking changes include:

* removal or redefinition of existing API arguments
* changing return values
* removing or modifying existing properties on an options argument
* adding or removing errors
* altering expected timing of an event
* changing the side effects of using a particular API

#### Breaking Changes and Deprecations

Existing stable public APIs that change in a backward-incompatible way must
undergo deprecation. The exceptions to this rule are:

* Adding or removing errors thrown or reported by a public API;
* Changing error messages for errors without error code;
* Altering the timing and non-internal side effects of the public API;
* Changes to errors thrown by dependencies of Node.js, such as V8;
* One-time exceptions granted by the TSC.

For more information, see [Deprecations](#deprecations).

#### Breaking Changes to Internal Elements

Breaking changes to internal elements may occur in semver-patch or semver-minor
commits. Take significant care when making and reviewing such changes. Make
an effort to determine the potential impact of the change in the ecosystem. Use
[Canary in the Goldmine](https://github.com/nodejs/citgm) to test such changes.
If a change will cause ecosystem breakage, then it is semver-major. Consider
providing a Public API in such cases.

#### Unintended Breaking Changes

Sometimes, a change intended to be non-breaking turns out to be a breaking
change. If such a change lands on the master branch, a Collaborator may revert
it. As an alternative to reverting, the TSC may apply the semver-major label
after-the-fact.

##### Reverting commits

Revert commits with `git revert <HASH>` or `git revert <FROM>..<TO>`. The
generated commit message will not have a subsystem and may violate line length
rules. That is OK. Append the reason for the revert and any `Refs` or `Fixes`
metadata. Raise a Pull Request like any other change.

### Introducing New Modules

Treat commits that introduce new core modules with extra care.

Check if the module's name conflicts with an existing ecosystem module. If it
does, choose a different name unless the module owner has agreed in writing to
transfer it.

If the new module name is free, register a placeholder in the module registry as
soon as possible. Link to the pull request that introduces the new core module
in the placeholder's `README`.

For pull requests introducing new core modules:

* Allow at least one week for review.
* Label with the `tsc-review` label.
* Land only after sign-off from at least two TSC members.
* Land with a [Stability Index][] of Experimental. The module must remain
  Experimental until a semver-major release.

### Additions to N-API

N-API provides an ABI stable API that we will have to support in future
versions without the usual option to modify or remove existing APIs on
SemVer boundaries. Therefore, additions need to be managed carefully.

This
[guide](https://github.com/nodejs/node/blob/master/doc/guides/adding-new-napi-api.md)
outlines the requirements and principles that we should follow when
approving and landing new N-API APIs (any additions to `node_api.h` and
`node_api_types.h`).

### Deprecations

[_Deprecation_][] is "the discouragement of use of some … feature … or practice,
typically because it has been superseded or is no longer considered efficient or
safe, without completely removing it or prohibiting its use. It can also imply
that a feature, design, or practice will be removed or discontinued entirely in
the future."

Node.js uses three Deprecation levels:

* *Documentation-Only Deprecation*: A deprecation notice is added to the API
  documentation but no functional changes are implemented in the code. By
  default, there will be no warnings emitted for such deprecations at
  runtime. Documentation-only deprecations may trigger a runtime warning when
  Node.js is started with the [`--pending-deprecation`][] flag or the
  `NODE_PENDING_DEPRECATION=1` environment variable is set.

* *Runtime Deprecation*: A warning is emitted at runtime the first time that a
  deprecated API is used. The [`--throw-deprecation`][] flag can be used to
  escalate such warnings into runtime errors that will cause the Node.js process
  to exit. As with Documentation-Only Deprecation, the documentation for the API
  must be updated to clearly indicate the deprecated status.

* *End-of-life*: The API is no longer subject to the semantic versioning rules.
  Backward-incompatible changes including complete removal of such APIs may
  occur at any time.

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
A _Deprecation cycle_ is a major release during which an API has been in one of
the three Deprecation levels. Documentation-Only Deprecations may land in a
minor release but must not be upgraded to a Runtime Deprecation until the next
major release.

No API can be moved to End-of-life without first having gone through a
Runtime Deprecation cycle. However, there is no requirement that deprecated
code must progress ultimately to *End-of-Life*. Documentation-only and runtime
deprecations may remain indefinitely.

Communicate pending deprecations and associated mitigations with the ecosystem
as soon as possible (preferably before the pull request adding the deprecation
lands on the master branch). Use the `notable-change` label on all pull requests
that add a new deprecation or move an existing deprecation to a new deprecation
level.

### Involving the TSC

Collaborators may opt to elevate pull requests or issues to the [TSC][].
This should be done where a pull request:

- is labeled `semver-major`, or
- has a significant impact on the codebase, or
- is inherently controversial, or
- has failed to reach consensus amongst the Collaborators who are
  actively participating in the discussion.

Assign the `tsc-review` label or @-mention the
`@nodejs/tsc` GitHub team if you want to elevate an issue to the [TSC][].
Do not use the GitHub UI on the right-hand side to assign to
`@nodejs/tsc` or request a review from `@nodejs/tsc`.

The TSC should serve as the final arbiter where required.

## Landing Pull Requests

1. Avoid landing PRs that are assigned to someone else. Authors who wish to land
   their own PRs will self-assign them, or delegate to someone else. If in
   doubt, ask the assignee whether it is okay to land.
1. Never use GitHub's green ["Merge Pull Request"][] button. Reasons for not
   using the web interface button:
   * The "Create a merge commit" method will add an unnecessary merge commit.
   * The "Squash and merge" method will add metadata (the PR #) to the commit
     title. If more than one author has contributed to the PR, squashing will
     only keep the most recent author.
   * The "Rebase and merge" method has no way of adding metadata to the commit.
1. Make sure the CI is done and the result is green. If the CI is not green,
   check for flaky tests and infrastructure failures. Please check if those were
   already reported in the appropriate repository ([node][flaky tests] and
   [build](https://github.com/nodejs/build/issues)) or not and open new issues
   in case they are not. If no CI was run or the run is outdated because code
   was pushed after the last run, please first start a new CI and wait for the
   result. If no CI is required, please leave a comment in case none is already
   present.
1. Review the commit message to ensure that it adheres to the guidelines
   outlined in the [contributing][] guide.
1. Add all necessary [metadata](#metadata) to commit messages before landing. If
   you are unsure exactly how to format the commit messages, use the commit log
   as a reference. See [this commit][commit-example] as an example.

For PRs from first-time contributors, be [welcoming](#welcoming-first-time-contributors).
Also, verify that their git settings are to their liking.

All commits should be self-contained, meaning every commit should pass all
tests. This makes it much easier when bisecting to find a breaking change.

### Using `git-node`

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
pick 51759dc crypto: feature B
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
pick 51759dc crypto: feature B
fixup 7d6f433 test for feature B
```

Replace `pick` with `reword` to change the commit message:

```text
reword 6928fc1 crypto: add feature A
fixup 8120c4c add test for feature A
reword 51759dc crypto: feature B
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
Don't manually close the PR, GitHub will close it automatically later after you
push it upstream, and will mark it with the purple merged status rather than the
red closed status. If you close the PR before GitHub adjusts its status, it will
show up as a 0 commit PR and the changed file history will be empty. Also if you
push upstream before you push to your branch, GitHub will close the issue with
red status so the order of operations is important.

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
To fix this, pull with rebase from upstream and run the tests again (to make
sure no interactions between your changes and the new changes cause any
problems), and push again:

```sh
git pull upstream master --rebase
make -j4 test
git push upstream master
```

### I Made a Mistake

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

Any Collaborator who feels a semver-minor commit should be landed in an LTS
branch should attach the `lts-agenda` label to the pull request. The LTS WG
will discuss the issue and, if necessary, will escalate the issue up to the
TSC for further discussion.

#### How are LTS Branches Managed?

There are multiple LTS branches, e.g. `v10.x` and `v8.x`. Each of these is
paired with a staging branch: `v10.x-staging` and `v8.x-staging`.

As commits land on the master branch, they are cherry-picked back to each
staging branch as appropriate. If the commit applies only to the LTS branch, the
PR must be opened against the *staging* branch. Commits are selectively
pulled from the staging branch into the LTS branch only when a release is
being prepared and may be pulled into the LTS branch in a different order
than they were landed in staging.

Only the members of the @nodejs/backporters team should land commits onto
LTS staging branches.

#### How can I help?

When you send your pull request, please include information about whether your
change is breaking. If you think your patch can be backported, please include
that information in the PR thread or your PR description. For more information
on backporting, please see the [backporting guide][].

Several LTS related issue and PR labels have been provided:

* `lts-watch-v10.x` - tells the LTS WG that the issue/PR needs to be
  considered for landing in the `v10.x-staging` branch.
* `lts-watch-v8.x` - tells the LTS WG that the issue/PR needs to be
  considered for landing in the `v8.x-staging` branch.
* `lts-watch-v6.x` - tells the LTS WG that the issue/PR needs to be
  considered for landing in the `v6.x-staging` branch.
* `land-on-v10.x` - tells the release team that the commit should be landed
  in a future v10.x release.
* `land-on-v8.x` - tells the release team that the commit should be landed
  in a future v8.x release.
* `land-on-v6.x` - tells the release team that the commit should be landed
  in a future v6.x release.

Any Collaborator can attach these labels to any PR/issue. As commits are
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

## Who to CC in the issue tracker

| Subsystem                                | Maintainers                                                           |
| ---                                      | ---                                                                   |
| `benchmark/*`                            | @nodejs/benchmarking, @mscdex                                         |
| `doc/*`, `*.md`                          | @nodejs/documentation                                                 |
| `lib/assert`                             | @nodejs/assert                                                        |
| `lib/async_hooks`                        | @nodejs/async\_hooks for bugs/reviews (+ @nodejs/diagnostics for API) |
| `lib/buffer`                             | @nodejs/buffer                                                        |
| `lib/child_process`                      | @nodejs/child\_process                                                |
| `lib/cluster`                            | @nodejs/cluster                                                       |
| `lib/{crypto,tls,https}`                 | @nodejs/crypto                                                        |
| `lib/dgram`                              | @nodejs/dgram                                                         |
| `lib/domains`                            | @nodejs/domains                                                       |
| `lib/fs`, `src/{fs,file}`                | @nodejs/fs                                                            |
| `lib/{_}http{*}`                         | @nodejs/http                                                          |
| `lib/inspector.js`, `src/inspector_*`    | @nodejs/v8-inspector                                                  |
| `lib/internal/bootstrap/*`               | @nodejs/process                                                       |
| `lib/internal/url`, `src/node_url`       | @nodejs/url                                                           |
| `lib/net`                                | @bnoordhuis, @indutny, @nodejs/streams                                |
| `lib/repl`                               | @nodejs/repl                                                          |
| `lib/{_}stream{*}`                       | @nodejs/streams                                                       |
| `lib/timers`                             | @nodejs/timers                                                        |
| `lib/util`                               | @nodejs/util                                                          |
| `lib/zlib`                               | @nodejs/zlib                                                          |
| `src/async_wrap.*`                       | @nodejs/async\_hooks                                                  |
| `src/node_api.*`                         | @nodejs/n-api                                                         |
| `src/node_crypto.*`                      | @nodejs/crypto                                                        |
| `test/*`                                 | @nodejs/testing                                                       |
| `tools/node_modules/eslint`, `.eslintrc` | @nodejs/linting                                                       |
| build                                    | @nodejs/build                                                         |
| `src/module_wrap.*`, `lib/internal/modules/*`, `lib/internal/vm/module.js` | @nodejs/modules                     |
| GYP                                      | @nodejs/gyp                                                           |
| performance                              | @nodejs/performance                                                   |
| platform specific                        | @nodejs/platform-{aix,arm,freebsd,macos,ppc,smartos,s390,windows}     |
| python code                              | @nodejs/python                                                        |
| upgrading c-ares                         | @rvagg                                                                |
| upgrading http-parser                    | @nodejs/http, @nodejs/http2                                           |
| upgrading libuv                          | @nodejs/libuv                                                         |
| upgrading npm                            | @fishrock123, @MylesBorins                                            |
| upgrading V8                             | @nodejs/V8, @nodejs/post-mortem                                       |
| Embedded use or delivery of Node.js      | @nodejs/delivery-channels                                             |

When things need extra attention, are controversial, or `semver-major`:
@nodejs/tsc

If you cannot find who to cc for a file, `git shortlog -n -s <file>` may help.

["Merge Pull Request"]: https://help.github.com/articles/merging-a-pull-request/#merging-a-pull-request-on-github
[Stability Index]: doc/api/documentation.md#stability-index
[TSC]: https://github.com/nodejs/TSC
[_Deprecation_]: https://en.wikipedia.org/wiki/Deprecation
[`--pending-deprecation`]: doc/api/cli.md#--pending-deprecation
[`--throw-deprecation`]: doc/api/cli.md#--throw-deprecation
[`node-core-utils`]: https://github.com/nodejs/node-core-utils
[backporting guide]: doc/guides/backporting-to-release-lines.md
[contributing]: ./doc/guides/contributing/pull-requests.md#commit-message-guidelines
[commit-example]: https://github.com/nodejs/node/commit/b636ba8186
[flaky tests]: https://github.com/nodejs/node/issues?q=is%3Aopen+is%3Aissue+label%3A%22CI+%2F+flaky+test%22y
[git-node]: https://github.com/nodejs/node-core-utils/blob/master/docs/git-node.md
[git-node-metadata]: https://github.com/nodejs/node-core-utils/blob/master/docs/git-node.md#git-node-metadata
[git-username]: https://help.github.com/articles/setting-your-username-in-git/
[git-email]: https://help.github.com/articles/setting-your-commit-email-address-in-git/
[node-core-utils-credentials]: https://github.com/nodejs/node-core-utils#setting-up-credentials
[node-core-utils-issues]: https://github.com/nodejs/node-core-utils/issues
