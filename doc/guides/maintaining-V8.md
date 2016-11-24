# Maintaining V8 in Node

# Background

V8 follows the Chromium release schedule. The support horizon for Chromium is very different from the support horizon that Node.js needs to provide to its users. As a result Node.js needs to support a version of V8 for quite a bit longer than what upstream needs to support. Since V8 doesn't have an LTS supported branch, there is no official process around how the V8 branches in Node are maintained.

This document attempts to document the current processes and proposes a workflow for maintaining the V8 branches in Node.js LTS and Current releases and how the Node and V8 teams at Google can help.

# V8 Release Schedule

V8 and Chromium follow a [roughly 6-week release cadence](https://www.chromium.org/developers/calendar). At any given time there are three V8 branches that are **active**.

For example, at the time of this writing:

* **Stable**: V8 5.4 is currently shipping as part of Chromium stable. This branch was created approx. 6 weeks before from when V8 5.3 shipped as stable.
* **Beta**: V8 5.5 is currently in beta. It will be promoted to stable next; approximately 6 weeks after V8 5.4 shipped as stable.
* **Master**: V8 tip-of-tree corresponds to V8 5.6. This branch gets regularly released as part of the Chromium **canary** builds. This branch will be promoted to beta next when V8 5.5 ships as stable.

All older branches are considered **abandoned**, and are not maintained by the V8 team.

## V8 merge process overview

The process for backporting bug fixes to active branches is officially documented [on the V8 wiki](https://github.com/v8/v8/wiki/Merging%20&%20Patching). The summary of the process is:

*   V8 only supports active branches. There is no testing done on any branches older than the current stable/beta/master.
*   A fix needing backport is tagged w/ *merge-request-x.x* tag. This can be done by anyone interested in getting the fix backported. Issues with this tag are reviewed by the V8 team regularly as candidates for backporting.
*   Fixes need some 'baking time' before they can be approved for backporting. This means waiting a few days to ensure that no issues are detected on the canary/beta builds.
*   Once ready, the issue is tagged w/ *merge-approved-x.x* and one can do the actual merge by using the scripts on the [wiki page](https://github.com/v8/v8/wiki/Merging%20&%20Patching).
*   Merge requests to an abandoned branch will be rejected.
*   Only bug fixes are accepted for backporting.

# Node Support Requirements

At any given time Node needs to be maintaining a few different V8 branches for the various Current, LTS, and nightly releases. At present this list includes the following branches<sup>1</sup>:

<table>
  <tr>
   <td><strong>Release</strong>
   </td>
   <td><strong>Support Start</strong>
   </td>
   <td><strong>Support End</strong>
   </td>
   <td><strong>V8 version</strong>
   </td>
   <td><strong>V8 branch released</strong>
   </td>
   <td><strong>V8 branch abandoned</strong>
   </td>
  </tr>
  <tr>
   <td>Node v4.x
   </td>
   <td>2015-10-01
   </td>
   <td>2018-04-01
   </td>
   <td>4.5
   </td>
   <td>2015-09-01
   </td>
   <td>2015-10-13
   </td>
  </tr>
  <tr>
   <td>Node v6.x
   </td>
   <td>2016-04-01
   </td>
   <td>2019-04-01
   </td>
   <td>5.1
   </td>
   <td>2016-05-31
   </td>
   <td>2016-06-26
   </td>
  </tr>
  <tr>
   <td>Node v7.x
   </td>
   <td>2016-10-01
   </td>
   <td>2017-04-01
   </td>
   <td>5.4
   </td>
   <td>2016-10-18
   </td>
   <td>~2016-12-01
   </td>
  </tr>
  <tr>
   <td>master
   </td>
   <td>N/A
   </td>
   <td>N/A
   </td>
   <td>5.4
   </td>
   <td>2016-10-18
   </td>
   <td>~2016-12-01
   </td>
  </tr>
</table>


The versions of V8 used in Node v4.x and v6.x have already been abandoned by upstream V8. However, Node.js needs to continue supporting these branches for many months (Current branches) or several years (LTS branches).

# Maintenance Process

Once a bug in Node.js has been identified to be caused by V8, the first step is to identify the versions of Node and V8 affected. The bug may be present in multiple different locations, each of which follows a slightly different process.

* Unfixed bugs. The bug exists in the V8 master branch.
* Fixed, but needs backport. The bug may need porting to one or more branches.
    * Backporting to active branches.
    * Backporting to abandoned branches.
* Backports identified by the V8 team. Bugs identified by upstream V8 that we haven't encountered in Node yet.

## Unfixed Upstream Bugs

If the bug can be reproduced on the [`vee-eight-lkgr` branch](https://github.com/v8/node/tree/vee-eight-lkgr), Chromium canary, or V8 tip-of-tree, and the test case is valid, then the bug needs to be fixed upstream first.

* Start by opening a bug upstream [using this template](https://bugs.chromium.org/p/v8/issues/entry?template=Node.js%20upstream%20bug).
* Make sure to include a link to the corresponding Node.js issue (if one exists).
* If the fix is simple enough, you may fix it yourself; [contributions](https://github.com/v8/v8/wiki/Contributing) are welcome.
* V8's build waterfall tests your change.
* Once the bug is fixed it may still need backporting, if it exists in other V8 branches that are still active or are branches that Node cares about. Follow the process for backporting below.

## Backporting to Active Branches

If the bug exists in any of the active V8 branches, we may need to get the fix backported. At any given time there are [two active branches](https://build.chromium.org/p/client.v8.branches/console) (beta and stable) in addition to master. The following steps are needed to backport the fix:

* Identify which version of V8 the bug was fixed in.
* Identify if any active V8 branches still contain the bug:
* A tracking bug is needed to request a backport.
    * If there isn't already a V8 bug tracking the fix, open a new merge request bug using this [Node.js specific template](https://bugs.chromium.org/p/v8/issues/entry?template=Node.js%20merge%20request).
    * If a bug already exists
        * Add a reference to the GitHub issue.
        * Attach *merge-request-x.x* labels to the bug for any active branches that still contain the bug. (e.g. merge-request-5.3, merge-request-5.4)
        * Add ofrobots-at-google.com to the cc list.
* Once the merge has been approved, it should be merged using the [merge script documented in the V8 wiki](https://github.com/v8/v8/wiki/Merging%20&%20Patching). Merging requires commit access to the V8 repository. If you don't have commit access you can indicate someone on the V8 team can do the merge for you.
* It is possible that the merge request may not get approved, for example if it is considered to be a feature or otherwise too risky for V8 stable. In such cases we float the patch on the Node side. See the process on 'Backporting to Abandoned branches'.
* Once the fix has been merged upstream, it can be picked up during an update of the V8 branch, (see below).

## Backporting to Abandoned Branches

Abandoned V8 branches are supported in the Node.js V8 repository. The fix needs to be cherry-picked in the Node.js repository and V8-CI must test the change.

* For each abandoned V8 branch corresponding to an LTS branch that is affected by the bug:
    * Open a cherry-pick PR on nodejs/node targeting the appropriate *vY.x-staging* branch (e.g. *v6.x-staging* to fix an issue in V8-5.1).
    * Increase the patch level version in v8-version.h. This will not cause any problems with versioning because V8 will not publish other patches for this branch, so Node.js can effectively bump the patch version.
    * In some cases the patch may require extra effort to merge in case V8 has changed substantially. For important issues we may be able to lean on the V8 team to get help with reimplementing the patch.
    * Run Node's [V8-CI](https://ci.nodejs.org/job/node-test-commit-v8-linux/) in addition to the [Node CI](https://ci.nodejs.org/job/node-test-pull-request/).

An example for workflow how to cherry-pick consider the following bug: https://crbug.com/v8/5199. From the bug we can see that it was merged by V8 into 5.2 and 5.3, and not into V8 5.1 (since it was already abandoned). Since Node.js `v6.x` uses V8 5.1, the fix needed to cherry-picked. To cherry-pick, here's an example workflow:

* Download and apply the commit linked-to in the issue (in this case a51f429). `curl -L https://github.com/v8/v8/commit/a51f429.patch | git apply --directory=deps/v8`. If the branches have diverged significantly, this may not apply cleanly. It may help to try to cherry-pick the merge to the oldest branch that was done upstream in V8. In this example, this would be the patch from the merge to 5.2. The hope is that this would be closer to the V8 5.1, and has a better chance of applying cleanly. If you're stuck, feel free to ping @ofrobots for help.
* Modify the commit message to match the format we use for V8 backports. You may want to add extra description if necessary to indicate the impact of the fix on Node. In this case the original issue was descriptive enough. Example:
```
deps: cherry-pick a51f429 from V8 upstream

Original commit message:
  [regexp] Fix case-insensitive matching for one-byte subjects.

  The bug occurs because we do not canonicalize character class ranges
  before adding case equivalents. While adding case equivalents, we abort
  early for one-byte subject strings, assuming that the ranges are sorted.
  Which they are not.

  R=marja@chromium.org
  BUG=v8:5199

  Review-Url: https://codereview.chromium.org/2159683002
  Cr-Commit-Position: refs/heads/master@{#37833}

PR-URL: <pr link>
```
* Open a PR against the `v6.x-staging` branch in the Node.js repo. Launch the normal and [V8-CI](https://ci.nodejs.org/job/node-test-commit-v8-linux/) using the Node.js CI system. We only needed to backport to `v6.x` as the other LTS branches weren't affected by this bug.

## Backports Identified by the V8 team

For bugs found through the browser or other channels, the V8 team marks bugs that might be applicable to the abandoned branches in use by Node.js. This is done through manual tagging by the V8 team and through an automated process that tags any fix that gets backported to the stable branch (as it is likely candidate for backporting further).

Such fixes are tagged with the following labels in the V8 issue tracker:

*   `NodeJS-Backport-Review` ([V8](https://bugs.chromium.org/p/v8/issues/list?can=1&q=label%3ANodeJS-Backport-Review), [Chromium](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3ANodeJS-Backport-Review)): to be reviewed if this is applicable to abandoned branches in use by Node.js. This list if regularly reviewed by the node team at Google to determine applicability to Node.js.
*   `NodeJS-Backport-Approved` ([V8](https://bugs.chromium.org/p/v8/issues/list?can=1&q=label%3ANodeJS-Backport-Approved), [Chromium](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3ANodeJS-Backport-Approved)): marks bugs that are deemed relevant to Node.js and should be backported.
*   `NodeJS-Backport-Done` ([V8](https://bugs.chromium.org/p/v8/issues/list?can=1&q=label%3ANodeJS-Backport-Done), [Chromium](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3ANodeJS-Backport-Done)): Backport for Node.js has been performed already.
*   `NodeJS-Backport-Rejected` ([V8](https://bugs.chromium.org/p/v8/issues/list?can=1&q=label%3ANodeJS-Backport-Rejected), [Chromium](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=label%3ANodeJS-Backport-Rejected)): Backport for Node.js is not desired.

The backlog of issues with such is regularly reviewed by the node-team at Google to shepherd through the backport process. External contributors are welcome to collaborate on the backport process as well. Note that some of the bugs may be security issues and will not be visible to external collaborators.

# Updating V8

Node keeps a vendored copy of V8 inside of deps/ directory. In addition Node may need to float patches that do not exist upstream. This means that some care may need to be taken to update the vendored copy of V8.

## Minor updates (patch level)

Because there may be floating patches on the version of V8 in Node.js, it is safest to apply the patch level updates as a patch. For example, imagine that upstream V8 is at 5.0.71.47 and Node.js is at 5.0.71.32. It would be best to compute the diff between these tags on the V8 repository, and then apply that patch on the copy of V8 in Node.js. This should preserve the patches/backports that Node.js may be floating (or else cause a merge conflict).

The rough outline of the process is:

```shell
# Assuming your fork of Node.js is checked out in $NODE_DIR
# and you want to update the Node.js master branch.
# Find the current (OLD) version in
# $NODE_DIR/deps/v8/include/v8-version.h
cd $NODE_DIR
git checkout master
git merge --ff-only origin/master
git checkout -b V8_NEW_VERSION
curl -L https://github.com/v8/v8/compare/${V8_OLD_VERSION}...${V8_NEW_VERSION}.patch | git apply --directory=deps/v8
# You may want to amend the commit message to describe the nature of the update
```

V8 also keeps tags of the form *5.4-lkgr* which point to the *Last Known Good Revision* from the 5.4 branch that can be useful in the update process above.


## Major Updates

We upgrade the version of V8 in Node.js master whenever a V8 release goes stable upstream, that is, whenever a new release of Chrome comes out.

Upgrading major versions would be much harder to do with the patch mechanism above. A better strategy is to

1. Audit the current master branch and look at the patches that have been floated since the last major V8 update.
1. Replace the copy of V8 in Node.js with a fresh checkout of the latest stable V8 branch. Special care must be taken to recursively update the DEPS that V8 has a compile time dependency on (at the moment of this writing, these are only trace_event and gtest_prod.h)
1. Refloat (cherry-pick) all the patches from list computed in 1) as necessary. Some of the patches may no longer be necessary.

To audit for floating patches:

```shell
git log --oneline deps/v8
```

To replace the copy of V8 in Node, use the '[update-v8](https://gist.github.com/targos/8da405e96e98fdff01a395bed365b816)' script<sup>2</sup>. For example, if you want to replace the copy of V8 in Node.js with the branch-head for V8 5.1 branch:

```shell
cd $NODE_DIR
rm -rf deps/v8
path/to/update-v8 branch-heads/5.1
```

You may want to look at the commits created by the above scripts, and squash them once you have reviewed them.

This should be followed up with manual refloating of all relevant patches.

# Proposal: Using a fork repo to track upstream V8

The fact that Node.js keeps a vendored, potentially edited copy of V8 in deps/ makes the above processes a bit complicated. An alternative proposal would be to create a fork of V8 at nodejs/v8 that would be used to maintain the V8 branches. This has several benefits:

* The process to update the version of V8 in Node.js could be automated to track the tips of various V8 branches in nodejs/v8.
* It would simplify cherry-picking and porting of fixes between branches as the version bumps in v8-version.h would happen as part of this update instead of on every change.
* It would simplify the V8-CI and make it more automatable.
* The history of the V8 branch in nodejs/v8 becomes purer and it would make it easier to pull in the V8 team for help with reviewing.
* It would make it simpler to setup an automated build that tracks Node.js master + V8 lkgr integration build.

This would require some tooling to:

* A script that would update the V8 in a specific Node branch with V8 from upstream (dependent on branch abandoned vs. active).
* We need a script to bump V8 version numbers when a new version of V8 is promoted from nodejs/v8 to nodejs/node.
* Enabled the V8-CI build in Jenkins to build from the nodejs/v8 fork.

# Proposal: Dealing with the need to float patches to a stable/beta

Sometimes upstream V8 may not want to merge a fix to their stable branches, but we might. An example of this would be a fix for a performance regression that only affects Node.js and not the browser. At the moment we don't have a mechanism to deal with this situation. If we float a patch and bump the V8 version, we might run into a problem if upstream releases a fix with the same version number.

One idea we have been kicking around is that we could move to a 5-place version number in V8, e.g.: 5.4.500.30.${embedder}. The ${embedder} represents the number of patches an embedder is floating on top of an official V8 version. This would also help with auditing the floating patches in the Node commit history.

We are trying this out in https://github.com/nodejs/node/pull/9754. If this ends up working, we will investigate making this change upstream.

<!-- Footnotes themselves at the bottom. -->
## Notes

<sup>1</sup>Node.js 0.12 and older are intentionally omitted from this document as their support is ending soon.

<sup>2</sup>It seems that @targos is working on port of this script here https://github.com/targos/update-v8.
