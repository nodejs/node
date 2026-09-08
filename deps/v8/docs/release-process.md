---
title: 'Release process'
description: 'This document explains the V8 release process.'
---
The V8 release process is tightly connected to [Chrome’s](https://www.chromium.org/getting-involved/dev-channel). The V8 team is using all four Chrome release channels to push new versions to the users.

If you want to look up what V8 version is in a Chrome release you can check [Chromiumdash](https://chromiumdash.appspot.com/releases). For each Chrome release a separate branch is created in the V8 repository to make the trace-back easier e.g. for [Chrome M121](https://chromium.googlesource.com/v8/v8/+log/refs/branch-heads/12.1).

## Canary releases

Every day a new Canary build is pushed to the users via [Chrome’s Canary channel](https://www.google.com/chrome/browser/canary.html?platform=win64). Normally the deliverable is the latest, stable enough version from [main](https://chromium.googlesource.com/v8/v8.git/+/refs/heads/main).

Branches for a Canary normally look like this:

## Dev releases

Every week a new Dev build is pushed to the users via [Chrome’s Dev channel](https://www.google.com/chrome/browser/desktop/index.html?extra=devchannel&platform=win64). Normally the deliverable includes the latest stable enough V8 version on the Canary channel.


## Beta releases

Roughly every 2 weeks a new major branch is created e.g. [for Chrome 94](https://chromium.googlesource.com/v8/v8.git/+log/branch-heads/9.4). This is happening in sync with the creation of [Chrome’s Beta channel](https://www.google.com/chrome/browser/beta.html?platform=win64). The Chrome Beta is pinned to the head of V8’s branch. After approx. 2 weeks the branch is promoted to Stable.

Changes are only cherry-picked onto the branch in order to stabilize the version.

Branches for a Beta normally look like this

```
refs/branch-heads/12.1
```

They are based on a Canary branch.

## Stable releases

Roughly every 4 weeks a new major Stable release is done. No special branch is created as the latest Beta branch is simply promoted to Stable. This version is pushed to the users via [Chrome’s Stable channel](https://www.google.com/chrome/browser/desktop/index.html?platform=win64).

Branches for a Stable release normally look like this:

```
refs/branch-heads/12.1
```

They are promoted (reused) Beta branches.

## API

Chromiumdash is also providing an API to collect the same information:

```
https://chromiumdash.appspot.com/fetch_milestones (to get the V8 branch name e.g. refs/branch-heads/12.1)
https://chromiumdash.appspot.com/fetch_releases (to get the the V8 branch git hash)
```

The following parameter are helpful:
mstone=121
channel=Stable,Canary,Beta,Dev
platform=Mac,Windows,Lacros,Linux,Android,Webview,etc.

## Which version should I embed in my application?

The tip of the same branch that Chrome’s Stable channel uses.

We often backmerge important bug fixes to a stable branch, so if you care about stability and security and correctness, you should include those updates too — that’s why we recommend “the tip of the branch”, as opposed to an exact version.

As soon as a new branch is promoted to Stable, we stop maintaining the previous stable branch. This happens every four weeks, so you should be prepared to update at least this often.

**Related:** [Which V8 version should I use?](/docs/version-numbers#which-v8-version-should-i-use%3F)
