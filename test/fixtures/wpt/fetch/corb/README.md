# Tests related to Cross-Origin Resource Blocking (CORB).

### Summary

This directory contains tests related to the
[Cross-Origin Resource Blocking (CORB)](https://chromium.googlesource.com/chromium/src/+/master/services/network/cross_origin_read_blocking_explainer.md)
algorithm.

The tests in this directory interact with various, random features,
but the tests have been grouped together into the `fetch/corb` directory,
because all of these tests verify behavior that is important to the CORB
algorithm.


### CORB is not universally implemented yet

CORB has been included
in the [Fetch spec](https://fetch.spec.whatwg.org/#corb)
since [May 2018](https://github.com/whatwg/fetch/pull/686).

Some tests in this directory (e.g.
`css-with-json-parser-breaker`) cover behavior spec-ed outside of CORB (making
sure that CORB doesn't change the existing web behavior) and therefore are
valuable independently from CORB's standardization efforts and should already
be passing across all browsers.

Tests that cover behavior that is changed by CORB are currently marked as
[tentative](https://web-platform-tests.org/writing-tests/file-names.html)
(using `.tentative` substring in their filename).
Such tests may fail unless CORB is enabled.  In practice this means that:
* Such tests will pass in Chromium
  (where CORB is enabled by default [since M68](https://crrev.com/553830)).
* Such tests may fail in other browsers.


### Limitations of WPT test coverage

CORB is a defense-in-depth and in general should not cause changes in behavior
that can be observed by web features or by end users.  This makes CORB difficult
or even impossible to test via WPT.

WPT tests can cover the following:

* Helping verify CORB has no observable impact in specific scenarios.
  Examples:
  * image rendering of (an empty response of) a html document blocked by CORB
    should be indistinguishable from rendering such html document without CORB -
    `img-html-correctly-labeled.sub.html`
  * CORB shouldn't block responses that don't sniff as a CORB-protected document
    type - `img-png-mislabeled-as-html.sub.html`
* Helping document cases where CORB causes observable changes in behavior.
  Examples:
  * blocking of nosniff images labeled as non-image, CORB-protected
    Content-Type - `img-png-mislabeled-as-html-nosniff.tentative.sub.html`
  * blocking of CORB-protected documents can prevent triggering
    syntax errors in scripts -
    `script-html-via-cross-origin-blob-url.tentative.sub.html`
* Helping verify which MIME types are protected by CORB.

Examples of aspects that WPT tests cannot cover (these aspects have to be
covered in other, browser-specific tests):
* Verifying that CORB doesn't affect things that are only indirectly
  observable by the web (like
  [prefetch](https://html.spec.whatwg.org/#link-type-prefetch).
* Verifying that CORB strips headers of blocked responses.
* Verifying that CORB blocks responses before they reach the process hosting
  a cross-origin execution context.
