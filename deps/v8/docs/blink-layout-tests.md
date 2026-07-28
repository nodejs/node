---
title: 'Blink web tests (a.k.a. layout tests)'
description: 'V8’s infrastructure continuously runs Blink’s web tests to prevent integration problems with Chromium. This document describes what to do in case such a test fails.'
---
We continuously run [Blink’s web tests (formerly known as “layout tests”)](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md) on our [integration console](https://ci.chromium.org/p/v8/g/integration/console) to prevent integration problems with Chromium.

On test failures, the bots compare the results of V8 Tip-of-Tree with Chromium’s pinned V8 version, to only flag newly introduced V8 problems (with false positives < 5%). Blame assignment is trivial as the [Linux release](https://ci.chromium.org/p/v8/builders/luci.v8.ci/V8%20Blink%20Linux) bot tests all revisions.

Commits with newly-introduced failures are normally reverted to unblock auto-rolling into Chromium. In case you notice you break layout tests or your commit gets reverted because of such breakage, and in case the changes are expected, follow this procedure to add updated baselines to Chromium before (re-)landing your CL:

1. Land a Chromium change setting `[ Failure Pass ]` for the changed tests ([more](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_test_expectations.md#updating-the-expectations-files)).
1. Land your V8 CL and wait 1-2 days until it cycles into Chromium.
1. Follow [these instructions](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md#Rebaselining-Web-Tests) to manually generate the new baselines. Note that if you’re making changes only to Chromium, [this preferred automatic procedure](https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_test_expectations.md#how-to-rebaseline) should work for you.
1. Remove the `[ Failure Pass ]` entry from the test expectations file and commit it along with the new baselines in Chromium.

Please associate all CLs with a `Bug: …` footer.
