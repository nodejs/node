Here you will find information that you'll need to be able to contribute to V8.  Be sure to read the whole thing before sending us a contribution, including the small print at the end.

## Before you contribute

Before you start working on a larger contribution V8 you should get in touch with us first through the V8 [contributor mailing list](http://groups.google.com/group/v8-dev) so we can help out and possibly guide you; coordinating up front makes it much easier to avoid frustration later on.

## Getting the code

See [UsingGit](using_git.md).

## Submitting code

The source code of V8 follows the [Google C++ Style Guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml) so you should familiarize yourself with those guidelines.  Before submitting code you must pass all our [tests](http://code.google.com/p/v8-wiki/wiki/Testing), and have to successfully run the presubmit checks:

> `tools/presubmit.py`

The presubmit script uses a linter from Google, `cpplint.py`.  External contributors can get this from [here](http://google-styleguide.googlecode.com/svn/trunk/cpplint/cpplint.py)  and place it in their path.

All submissions, including submissions by project members, require review.  We use the same code-review tools and process as the chromium project.  In order to submit a patch, you need to get the [depot\_tools](http://dev.chromium.org/developers/how-tos/install-depot-tools) and follow these instructions on [requesting a review](http://dev.chromium.org/developers/contributing-code) (using your V8 workspace instead of a chromium workspace).

### Look out for breakage or regressions

Before submitting your code please check the [buildbot console](http://build.chromium.org/p/client.v8/console) to see that the columns are mostly green before checking in your changes. Otherwise you will not know if your changes break the build or not. When your change is committed watch the [buildbot console](http://build.chromium.org/p/client.v8/console) until the bots turn green after your change.


## The small print

Before we can use your code you have to sign the [Google Individual Contributor License Agreement](http://code.google.com/legal/individual-cla-v1.0.html), which you can do online.  This is mainly because you own the copyright to your changes, even after your contribution becomes part of our codebase, so we need your permission to use and distribute your code.  We also need to be sure of various other things, for instance that you'll tell us if you know that your code infringes on other people's patents.  You don't have to do this until after you've submitted your code for review and a member has approved it, but you will have to do it before we can put your code into our codebase.

Contributions made by corporations are covered by a different agreement than the one above, the [Software Grant and Corporate Contributor License Agreement](http://code.google.com/legal/corporate-cla-v1.0.html).

Sign them online [here](https://cla.developers.google.com/)