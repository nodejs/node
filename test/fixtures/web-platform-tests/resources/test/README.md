# `testharness.js` test suite

The test suite for the `testharness.js` testing framework.

## Executing Tests

Install the following dependencies:

- [Python 2.7.9+](https://www.python.org/)
- [the tox Python package](https://tox.readthedocs.io/en/latest/)
- [the Mozilla Firefox web browser](https://mozilla.org/firefox)
- [the GeckoDriver server](https://github.com/mozilla/geckodriver)

Make sure `geckodriver` can be found in your `PATH`.

Currently, the tests should be run with the latest *Firefox Nightly*. In order to
specify the path to Firefox Nightly, use the following command-line option:

    tox -- --binary=/path/to/FirefoxNightly

### Automated Script

Alternatively, you may run `tools/ci/ci_resources_unittest.sh`, which only depends on
Python 2. The script will install other dependencies automatically and start `tox` with
the correct arguments.

## Authoring Tests

Test cases are expressed as `.html` files located within the `tests/unit/` and
`tests/funtional/` sub-directories. Each test should include the
`testharness.js` library with the following markup:

    <script src="/resources/testharness.js"></script>
    <script src="/resources/testharnessreport.js"></script>

This should be followed by one or more `<script>` tags that interface with the
`testharness.js` API in some way. For example:

    <script>
    test(function() {
        1 = 1;
      }, 'This test is expected to fail.');
    </script>

### Unit tests

The "unit test" type allows for concisely testing the expected behavior of
assertion methods. These tests may define any number of sub-tests; the
acceptance criteria is simply that all tests executed pass.

### Functional tests

Thoroughly testing the behavior of the harness itself requires ensuring a
number of considerations which cannot be verified with the "unit testing"
strategy. These include:

- Ensuring that some tests are not run
- Ensuring conditions that cause test failures
- Ensuring conditions that cause harness errors

Functional tests allow for these details to be verified. Every functional test
must include a summary of the expected results as a JSON string within a
`<script>` tag with an `id` of `"expected"`, e.g.:

    <script type="text/json" id="expected">
    {
      "summarized_status": {
        "message": null,
        "stack": null,
        "status_string": "OK"
      },
      "summarized_tests": [
        {
          "message": "ReferenceError: invalid assignment left-hand side",
          "name": "Sample HTML5 API Tests",
          "properties": {},
          "stack": "(implementation-defined)",
          "status_string": "FAIL"
        }
      ],
      "type": "complete"
    }
    </script>

`testharness.js` is expected to function consistently in a number of
distinct environments. In order to verify this expectation, each functional
test may be executed under a number of distinct conditions. These conditions
are applied using WPT's "test variants" pattern. The available variants are
defined in the `variants.js` file; this file must be included before
`testharness.js`. Every test must specify at least one variant.
