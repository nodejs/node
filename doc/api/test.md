# Test runner

<!--introduced_in=v18.0.0-->

<!-- YAML
added:
  - v18.0.0
  - v16.17.0
changes:
  - version: v20.0.0
    pr-url: https://github.com/nodejs/node/pull/46983
    description: The test runner is now stable.
-->

> Stability: 2 - Stable

<!-- source_link=lib/test.js -->

The `node:test` module facilitates the creation of JavaScript tests.
To access it:

```mjs
import test from 'node:test';
```

```cjs
const test = require('node:test');
```

This module is only available under the `node:` scheme.

Tests created via the `test` module consist of a single function that is
processed in one of three ways:

1. A synchronous function that is considered failing if it throws an exception,
   and is considered passing otherwise.
2. A function that returns a `Promise` that is considered failing if the
   `Promise` rejects, and is considered passing if the `Promise` fulfills.
3. A function that receives a callback function. If the callback receives any
   truthy value as its first argument, the test is considered failing. If a
   falsy value is passed as the first argument to the callback, the test is
   considered passing. If the test function receives a callback function and
   also returns a `Promise`, the test will fail.

The following example illustrates how tests are written using the
`test` module.

```js
test('synchronous passing test', (t) => {
  // This test passes because it does not throw an exception.
  assert.strictEqual(1, 1);
});

test('synchronous failing test', (t) => {
  // This test fails because it throws an exception.
  assert.strictEqual(1, 2);
});

test('asynchronous passing test', async (t) => {
  // This test passes because the Promise returned by the async
  // function is settled and not rejected.
  assert.strictEqual(1, 1);
});

test('asynchronous failing test', async (t) => {
  // This test fails because the Promise returned by the async
  // function is rejected.
  assert.strictEqual(1, 2);
});

test('failing test using Promises', (t) => {
  // Promises can be used directly as well.
  return new Promise((resolve, reject) => {
    setImmediate(() => {
      reject(new Error('this will cause the test to fail'));
    });
  });
});

test('callback passing test', (t, done) => {
  // done() is the callback function. When the setImmediate() runs, it invokes
  // done() with no arguments.
  setImmediate(done);
});

test('callback failing test', (t, done) => {
  // When the setImmediate() runs, done() is invoked with an Error object and
  // the test fails.
  setImmediate(() => {
    done(new Error('callback failure'));
  });
});
```

If any tests fail, the process exit code is set to `1`.

## Subtests

The test context's `test()` method allows subtests to be created.
It allows you to structure your tests in a hierarchical manner,
where you can create nested tests within a larger test.
This method behaves identically to the top level `test()` function.
The following example demonstrates the creation of a
top level test with two subtests.

```js
test('top level test', async (t) => {
  await t.test('subtest 1', (t) => {
    assert.strictEqual(1, 1);
  });

  await t.test('subtest 2', (t) => {
    assert.strictEqual(2, 2);
  });
});
```

> **Note:** `beforeEach` and `afterEach` hooks are triggered
> between each subtest execution.

In this example, `await` is used to ensure that both subtests have completed.
This is necessary because tests do not wait for their subtests to
complete, unlike tests created within suites.
Any subtests that are still outstanding when their parent finishes
are cancelled and treated as failures. Any subtest failures cause the parent
test to fail.

## Class: `SuiteContext`

<!-- YAML
added:
  - v18.7.0
  - v16.17.0
-->

An instance of `SuiteContext` is passed to each suite function in order to
interact with the test runner. However, the `SuiteContext` constructor is not
exposed as part of the API.

### `context.filePath`

<!-- YAML
added: v22.6.0
-->

The absolute path of the test file that created the current suite. If a test
file imports additional modules that generate suites, the imported suites will
return the path of the root test file.

### `context.fullName`

<!-- YAML
added: v22.3.0
-->

The name of the suite and each of its ancestors, separated by `>`.

### `context.name`

<!-- YAML
added:
  - v18.8.0
  - v16.18.0
-->

The name of the suite.

### `context.signal`

<!-- YAML
added:
  - v18.7.0
  - v16.17.0
-->

* Type: {AbortSignal}

Can be used to abort test subtasks when the test has been aborted.

[TAP]: https://testanything.org/
[`--experimental-test-coverage`]: cli.md#--experimental-test-coverage
[`--experimental-test-module-mocks`]: cli.md#--experimental-test-module-mocks
[`--import`]: cli.md#--importmodule
[`--no-strip-types`]: cli.md#--no-strip-types
[`--test-concurrency`]: cli.md#--test-concurrency
[`--test-coverage-exclude`]: cli.md#--test-coverage-exclude
[`--test-coverage-include`]: cli.md#--test-coverage-include
[`--test-name-pattern`]: cli.md#--test-name-pattern
[`--test-only`]: cli.md#--test-only
[`--test-reporter-destination`]: cli.md#--test-reporter-destination
[`--test-reporter`]: cli.md#--test-reporter
[`--test-rerun-failures`]: cli.md#--test-rerun-failures
[`--test-skip-pattern`]: cli.md#--test-skip-pattern
[`--test-update-snapshots`]: cli.md#--test-update-snapshots
[`--test`]: cli.md#--test
[`MockFunctionContext`]: #class-mockfunctioncontext
[`MockPropertyContext`]: #class-mockpropertycontext
[`MockTimers`]: #class-mocktimers
[`MockTracker.method`]: #mockmethodobject-methodname-implementation-options
[`MockTracker`]: #class-mocktracker
[`NODE_V8_COVERAGE`]: cli.md#node_v8_coveragedir
[`SuiteContext`]: #class-suitecontext
[`TestContext`]: #class-testcontext
[`context.diagnostic`]: #contextdiagnosticmessage
[`context.skip`]: #contextskipmessage
[`context.todo`]: #contexttodomessage
[`describe()`]: #describename-options-fn
[`glob(7)`]: https://man7.org/linux/man-pages/man7/glob.7.html
[`it()`]: #itname-options-fn
[`run()`]: #runoptions
[`suite()`]: #suitename-options-fn
[`test()`]: #testname-options-fn
[code coverage]: #collecting-code-coverage
[configuration files]: cli.md#--experimental-config-fileconfig
[describe options]: #describename-options-fn
[it options]: #testname-options-fn
[running tests from the command line]: #running-tests-from-the-command-line
[stream.compose]: stream.md#streamcomposestreams
[subtests]: #subtests
[suite options]: #suitename-options-fn
[test reporters]: #test-reporters
[test runner execution model]: #test-runner-execution-model
