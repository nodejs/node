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
added: REPLACEME
-->

The name of the suite and each of its ancestors, separated by `>`.

For example, if a suite named "Nested Suite 1" is nested inside a suite named
"Suite 1", the `fullName` would be `"Suite 1 > Nested Suite 1"`.

For top-level suites, this is the same as `context.name`.

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
