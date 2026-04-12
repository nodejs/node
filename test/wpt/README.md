# Web Platform Tests

This directory contains test runners that execute upstream
[Web Platform Tests][] against Node.js using the WPT harness.
The actual test files live in `test/fixtures/wpt`, a subset of the
upstream WPT repository containing only the modules relevant to Node.js.
Each module is updated independently using [git node wpt][], so
different modules may be pinned to different upstream commits.

Each module has a status file in the [`status` folder](./status) that
declares build requirements, expected failures, and tests to skip.
See [`test/fixtures/wpt/README.md`][] for the pinned WPT commit
hashes for each module.

<a id="add-tests"></a>

## How to add tests for a new module

### 1. Create a status file

For example, to add the URL tests, add a `test/wpt/status/url.cjs` file.

In the beginning, it's fine to leave an empty object `module.exports = {}`
in the file if it's not yet clear how compliant the implementation is,
the requirements and expected failures can be figured out in a later step
when the tests are run for the first time.

See [Format of a status file](#status-format) for details.

### 2. Pull the WPT files

Use the [git node wpt][] command to download the WPT files into
`test/fixtures/wpt`. For example, to add URL tests:

```bash
cd /path/to/node/project
git node wpt url
```

### 3. Create the test runner

For example, for the URL tests, add a file `test/wpt/test-url.js`:

```js
'use strict';

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

runner.pretendGlobalThisAs('Window');
runner.runJsTests();
```

The runner loads the tests from `test/fixtures/wpt/url`, applies the
status rules from `test/wpt/status/url.cjs`, and runs them using
worker threads.

#### `new WPTRunner(path[, options])`

* `path` {string} Relative path of the WPT module
  (e.g. `'url'`, `'html/webappapis/timers'`).
* `options` {Object}
  * `concurrency` {number} Number of tests to run in parallel.
    Defaults to `os.availableParallelism() - 1`. Set to `1` for tests
    that require sequential execution (e.g. web-locks, webstorage).

#### `runner.setFlags(flags)`

* `flags` {string\[]} Node.js CLI flags passed to each worker thread
  (e.g. `['--expose-internals']`).

#### `runner.setInitScript(script)`

* `script` {string} JavaScript code executed in the worker before
  the tests run. Useful for setting up globals needed by the tests.

#### `runner.setScriptModifier(modifier)`

* `modifier` {Function} A callback `(meta) => void` invoked for each
  script before it is run in the worker. `meta` is an object with
  `code` {string} and `filename` {string} properties that can be
  mutated.

#### `runner.pretendGlobalThisAs(name)`

* `name` {string} Currently only `'Window'` is supported. Sets up
  `globalThis.Window` so that WPT tests checking the global scope
  type work correctly.

#### `runner.runJsTests()`

Starts running the tests. Must be called last, after all configuration.

### 4. Run the tests

Run the test using `tools/test.py` and see if there are any failures.
For example, to run all the URL tests under `test/fixtures/wpt/url`:

```bash
tools/test.py wpt/test-url
```

To run a specific test in WPT, for example, `url/url-searchparams.any.js`,
pass the file name as argument to the corresponding test runner:

```bash
node test/wpt/test-url.js url-searchparams.any.js
```

If there are any failures, update the corresponding status file
(in this case, `test/wpt/status/url.cjs`) to make the test pass.

For example, to mark `url/url-searchparams.any.js` as expected to fail,
add this to `test/wpt/status/url.cjs`:

```js
module.exports = {
  'url-searchparams.any.js': {
    fail: {
      expected: [
        'test name in the WPT test case, e.g. second argument passed to test()',
      ],
    },
  },
};
```

See [Format of a status file](#status-format) for details.

### 5. Commit the changes and submit a Pull Request

See [the contributing guide](../../CONTRIBUTING.md).

## How to update tests for a module

The tests can be updated in a way similar to how they are added.
Run Step 2 and Step 4 of [adding tests for a new module](#add-tests).

The [git node wpt][] command maintains the status of the local
WPT subset. If no files are updated after running it for a module,
the local subset is up to date and there is no need to create a PR.
When files are updated, run the tests and update the status file to
account for any new failures or passes before submitting.

## Daily WPT report

A [GitHub Actions workflow][] runs every night and uploads results to
[wpt.fyi][]. It tests all active Node.js release lines and the latest
nightly build against the WPT `epochs/daily` branch, which is a daily
snapshot of the upstream WPT repository.

Unlike the pinned fixtures used in CI, this workflow replaces
`test/fixtures/wpt` with the full `epochs/daily` checkout so that
results reflect the latest upstream tests. Results can be viewed on
the [wpt.fyi dashboard][].

<a id="status-format"></a>

## Format of a status file

The status file can be either a `.json` file or a `.cjs` module that exports
the same object. Using CJS allows for conditional logic and regular
expressions, which JSON does not support.

```js
module.exports = {
  'something.scope.js': { // the file name
    // Optional: If the requirement is not met, this test will be skipped.
    // Supported values:
    //   'small-icu' - requires at least small-icu intl support
    //   'full-icu'  - requires full-icu intl support
    //   'crypto'    - requires crypto (OpenSSL) support
    //   'inspector' - requires the inspector to be available
    requires: ['small-icu'],

    // Optional: the entire file will be skipped with the reason printed.
    skip: 'explain why we cannot run a test that is supposed to pass',

    // Optional: failing tests.
    fail: {
      // Tests that are expected to fail consistently.
      expected: [
        'test name in the WPT test case, e.g. second argument passed to test()',
        'another test name',
      ],
      // Tests that fail intermittently. These are treated as expected
      // failures but are not flagged as unexpected passes when they
      // succeed.
      flaky: [
        'flaky test name',
      ],
    },
  },
};
```

A test should be marked with `skip` when it cannot be run at all, for
example, because it depends on a browser-only Web API or a harness feature
that has not been ported to the Node.js runner. Use `fail` instead when
the test can run but produces incorrect results due to an implementation
bug or missing feature.

### Skipping individual subtests

To skip specific subtests within a file (rather than skipping the entire file),
use `skipTests` with an array of exact test names or regular expressions:

```js
module.exports = {
  'something.scope.js': {
    skipTests: [
      'exact test name to skip',
      /regexp pattern to match/,
    ],
  },
};
```

Skipped subtests are reported as `[SKIP]` in the output, recorded as `NOTRUN`
in the WPT report, and counted separately in the summary line.

This is useful for skipping a particular subtest that crashes the runner,
which would otherwise prevent the rest of the file from being run. Using CJS
status files also enables conditionally skipping slow or resource-heavy
subtests in CI on specific architectures.

### Wildcard patterns in file names

File name keys can include a `*` character to match multiple test files
with a single entry. For example, to skip all `.window.js` tests:

```js
module.exports = {
  '*.window.js': {
    skip: 'window tests are not relevant for Node.js',
  },
};
```

The `*` is converted to a `.*` regular expression, so `"subdir/*.any.js"`
would match all `.any.js` files under the `subdir` directory. A test file
can match multiple rules (both an exact match and one or more wildcard
patterns); all matched rules are merged.

[GitHub Actions workflow]: ../../.github/workflows/daily-wpt-fyi.yml
[Web Platform Tests]: https://github.com/web-platform-tests/wpt
[`test/fixtures/wpt/README.md`]: ../fixtures/wpt/README.md
[git node wpt]: https://github.com/nodejs/node-core-utils/blob/HEAD/docs/git-node.md#git-node-wpt
[wpt.fyi]: https://wpt.fyi
[wpt.fyi dashboard]: https://wpt.fyi/results/?label=master&label=experimental&product=node.js&product=chrome&product=firefox&product=safari&product=ladybird&product=servo&q=node.js%3A%21missing
