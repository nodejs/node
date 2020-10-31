# Contributing to Perfetto
This project uses [Android AOSP Gerrit][perfetto-gerrit] for code reviews,
uses the [Google C++ style][google-cpp-style] and targets `-std=c++11`.

Development happens in this repo:
https://android.googlesource.com/platform/external/perfetto/

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code Reviews

All submissions, including submissions by project members, require review.
We use [Android AOSP Gerrit][perfetto-gerrit] for this purpose.

`git cl upload` from [Chromium depot tools][depot-tools] is the preferred
workflow to upload patches, as it supports presubmits and code formatting via
`git cl format`.

## Continuous integration

Continuous build and test coverage is available at
[ci.perfetto.dev](https://ci.perfetto.dev).

**Trybots**:  
CLs uploaded to Gerrit are automatically submitted to the CI and
and available on the CI page.
If the label `Presubmit-Ready: +1` is set, the CI will also publish a comment
like [this][ci-example] on the CL.

## Community

You can reach us on our [Discord channel](https://discord.gg/35ShE3A).
If you prefer using IRC we have an experimental Discord <> IRC bridge
synced with `#perfetto-dev` on [Freenode](https://webchat.freenode.net/).

This project follows
[Google's Open Source Community Guidelines](https://opensource.google/conduct/).

[perfetto-gerrit]: https://android-review.googlesource.com/q/project:platform%252Fexternal%252Fperfetto+status:open
[google-cpp-style]: https://google.github.io/styleguide/cppguide.html
[depot-tools]: https://dev.chromium.org/developers/how-tos/depottools
[ci-example]: https://android-review.googlesource.com/c/platform/external/perfetto/+/1108253/3#message-09fd27fb92ca8357abade3ec725919ac3445f3af
