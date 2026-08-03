For [Resource Timing][1] tests, we want to have a consistent and clear coding
style. The goals of this style are to:
*   Make it easier for new contributors to find their way around
*   Help improve readability and maintainability
*   Help us understand which parts of the spec are tested or not
Lots of the following rules are arbitrary but the value is realized in
consistency instead of adhering to the 'perfect' style.

We want the test suite to be navigable. Developers should be able to easily
find the file or test that is relevant to their work.
*   Tests should be arranged in files according to which piece of the spec they
    test
*   Files should be named using a consistent pattern
*   HTML files should include useful meta tags
    *   `<title>` for controlling labels in results pages
    *   `<link rel="help">` to point at the relevant piece of the spec

We want the test suite to run consistently. Flaky tests are counterproductive.
*   Prefer `promise_test` to `async_test`
    *   Note that there’s [still potential for some concurrency][2]; use
        `add_cleanup()` if needed

We want the tests to be readable. Tests should be written in a modern style
with recurring patterns.
*   80 character line limits where we can
*   Consistent use of anonymous functions
    *   prefer
        ```
        const func1 = param1 => {
          body();
        }
        const func2 = (param1, param2) => {
          body();
        }
        fn(param => {
            body();
        });
        ```

        over

        ```
        function func1(param1) {
          body();
        }
        function func2(param1, param2) {
          body();
        }
        fn(function(param) {
            body();
        });
        ```

*   Prefer `const` (or, if needed, `let`) to `var`
*   Contain use of ‘.sub’ in filenames to known helper utilities where possible
    *   E.g. prefer use of get-host-info.sub.js to `{{host}}` or `{{ports[0]}}`
        expressions
*   Avoid use of webperftestharness[extension].js as it’s a layer of cognitive
    overhead between test content and test intent
    *   Helper .js files are still encouraged where it makes sense but we want
        to avoid a testing framework that is specific to Resource Timing (or
        web performance APIs, in general).
*   Prefer [`fetch_tests_from_window`][3] to collect test results from embedded
    iframes instead of hand-rolled `postMessage` approaches
*   Use the [`assert_*`][4] family of functions to check conformance to the spec
    but throw exceptions explicitly when the test itself is broken.
    *    A failed assert indicates "the implementation doesn't conform to the
         spec"
    *    Other uncaught exceptions indicate "the test case itself has a bug"
*   Where possible, we want tests to be scalable - adding another test case
    should be as simple as calling the tests with new parameters, rather than
    copying an existing test and modifying it.

[1]: https://www.w3.org/TR/resource-timing-2/
[2]: https://web-platform-tests.org/writing-tests/testharness-api.html#promise-tests
[3]: https://web-platform-tests.org/writing-tests/testharness-api.html#consolidating-tests-from-other-documents
[4]: https://web-platform-tests.org/writing-tests/testharness-api.html#list-of-assertions
