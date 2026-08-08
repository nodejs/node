---
title: 'Merging & patching'
description: 'This document explains how to merge V8 patches to a release branch.'
---
If you have a patch to the `main` branch (e.g. an important bug fix) that needs to be merged into one of the release V8 branches (refs/branch-heads/12.5), read on.

The following examples use a branched 12.3 version of V8. Substitute `12.3` with your version number. Read the documentation on [V8’s version numbering](/docs/version-numbers) for more information.

An associated issue on V8’s issue tracker is **mandatory** if a patch is merged. This helps with keeping track of merges.

## What qualifies a merge candidate?

- The patch fixes a *severe* bug (in order of importance):
    1. security bug
    1. stability bug
    1. correctness bug
    1. performance bug
- The patch does not alter APIs.
- The patch does not change behavior present before branch cut (except if the behavior change fixes a bug).

More information can be found on the [relevant Chromium page](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/merge_request.md). When in doubt, send an email to <v8-dev@googlegroups.com>.

## The merge process

The merge process in the V8 tracker is driven by Attributes. Therefore please set the 'Merge-Request' on to the relevant Chrome Milestone. In case the merge is only affecting a V8 [port](https://v8.dev/docs/ports) please set the HW attribute accordingliy. E.g:

```
Merge-Request: 123
HW: MIPS,LoongArch64
```

once reviewed, this will be adjusted during the review to:

```
Merge: Approved-123
or
Merge: Rejected-123
```

After the CL landed, this will be adjusted one more time to:

```
Merge: Merged-123, Merged-12.3
```

## How to check if a commit was already merged/reverted/has Canary coverage

Use [chromiumdash](https://chromiumdash.appspot.com/commit/) to verify if the relevant CL has Canary coverage.


On top the **Releases** section should show a Canary.

## How to create the merge CL

### Option 1: Using [gerrit](https://chromium-review.googlesource.com/) - Recommended


1. Open the CL you want to back-merge.
1. Select "Cherry pick" from the extended menu (three vertical dots in the upper right corner).
1. Enter "refs/branch-heads/*XX.X*" as destination branch (replace *XX.X* by the proper branch).
1. Modify the commit message and prefix the title with "Merged: ".
1. In case of merge conflict, please also go ahead and create the CL. To resolve conflicts (if any) - either using the gerrit UI or you can easily pull the patch locally by using the "download patch" command from the menu (three vertical dots in the upper right corner).
1. Send out for review.

### Option 2: Using the automated script

Let’s assume you’re merging revision af3cf11 to branch 12.2 (please specify full git hashes - abbreviations are used here for simplicity).

```
https://crsrc.org/c/v8/tools/release/merge_to_branch_gerrit.py --branch 12.3 -r af3cf11
```


### After landing: Observe the [branch waterfall](https://ci.chromium.org/p/v8)

If one of the builders is not green after handling your patch, revert the merge immediately. A bot (`AutoTagBot`) takes care of the correct versioning after a 10-minute wait.

## Patching a version used on Canary/Dev

In case you need to patch a Canary/Dev version (which should not happen often), cc vahl@ or machenbach@ on the issue. Googlers: please check out the [internal site](http://g3doc/company/teams/v8/patching_a_version) before creating the CL.

