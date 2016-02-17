# Introduction

The V8 release process is tightly connected to [Chrome's](https://www.chromium.org/getting-involved/dev-channel). The V8 team is using all four Chrome release channels to push new versions to the users.

If you want to look up what V8 version is in a Chrome release you can check [OmahaProxy](https://omahaproxy.appspot.com/). For each Chrome release a separate branch is created in the V8 repository to make the trace-back easier e.g. for [Chrome 45.0.2413.0](https://chromium.googlesource.com/v8/v8.git/+/chromium/2413).

# Canary releases
Every day a new Canary build is pushed to the users via [Chrome's Canary channel](https://www.google.com/chrome/browser/canary.html?platform=win64). Normally the deliverable is the latest, stable enough version from [master](https://chromium.googlesource.com/v8/v8.git/+/roll).

Branches for a Canary normally look like this

```
remotes/origin/4.5.35
```

# Dev releases
Every week a new Dev build is pushed to the users via [Chrome's Dev channel](https://www.google.com/chrome/browser/desktop/index.html?extra=devchannel&platform=win64). Normally the deliverable includes the latest stable enough V8 version on the Canary channel.

Branches for a Dev normally look like this

```
remotes/origin/4.5.35
```

# Beta releases
Roughly every 6 weeks a new major branch is created e.g. [for Chrome 44](https://chromium.googlesource.com/v8/v8.git/+log/branch-heads/4.4). This is happening in sync with the creation of [Chrome's Beta channel](https://www.google.com/chrome/browser/beta.html?platform=win64). The Chrome Beta is pinned to the head of V8's branch. After approx. 6 weeks the branch is promoted to Stable.

Changes are only cherry-picked onto the branch in order to stabilize the version.

Branches for a Beta normally look like this

```
remotes/branch-heads/4.5
```

They are based on a Canary branch.

# Stable releases
Roughly every 6 weeks a new major Stable release is done. No special branch is created as the latest Beta branch is simply promoted to Stable. This version is pushed to the users via [Chrome's Stable channel](https://www.google.com/chrome/browser/desktop/index.html?platform=win64).

Branches for a Stable normally look like this

```
remotes/branch-heads/4.5
```

They are promoted (reused) Beta branches.

# Which version should I embed in my application?

The tip of the same branch that Chrome's Stable channel uses.

We often backmerge important bug fixes to a stable branch, so if you care about stability and security and correctness, you should include those updates too -- that's why we recommend "the tip of the branch", as opposed to an exact version.

As soon as a new branch is promoted to Stable, we stop maintaining the previous stable branch. This happens every six weeks, so you should be prepared to update at least this often.

Example: The current stable Chrome release is [44.0.2403.125](https://omahaproxy.appspot.com), with V8 4.4.63.25. So you should embed [branch-heads/4.4](https://chromium.googlesource.com/v8/v8.git/+/branch-heads/4.4). And you should update to branch-heads/4.5 when Chrome 45 is released on the Stable channel.