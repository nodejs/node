# Test runner

<!--introduced_in=v16.17.0-->

> Stability: 1 - Experimental

<!-- source_link=lib/test.js -->

The `node:test` module facilitates the creation of JavaScript tests that
report results in [TAP][] format. To access it:

```mjs
import test from 'node:test';
```

```cjs
const test = require('node:test');
```

This module is only available under the `node:` scheme. The following will not
work:

```mjs
import test from 'test';
```

```cjs
const test = require('test');
```

Tests created via the `test` module consist of a single function that is
processed in one of three ways:

1. A synchronous function that is considered failing if it throws an exception,
   and is considered passing otherwise.
2. A function that returns a `Promise` that is considered failing if the
   `Promise` rejects, and is considered passing if the `Promise` resolves.
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
  // function is not rejected.
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

As a test file executes, TAP is written to the standard output of the Node.js
process. This output can be interpreted by any test harness that understands
the TAP format. If any tests fail, the process exit code is set to `1`.

## Subtests

The test context's `test()` method allows subtests to be created. This method
behaves identically to the top level `test()` function. The following example
demonstrates the creation of a top level test with two subtests.

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

In this example, `await` is used to ensure that both subtests have completed.
This is necessary because parent tests do not wait for their subtests to
complete. Any subtests that are still outstanding when their parent finishes
are cancelled and treated as failures. Any subtest failures cause the parent
test to fail.

## Skipping tests

Individual tests can be skipped by passing the `skip` option to the test, or by
calling the test context's `skip()` method. Both of these options support
including a message that is displayed in the TAP output as shown in the
following example.

```js
// The skip option is used, but no message is provided.
test('skip option', { skip: true }, (t) => {
  // This code is never executed.
});

// The skip option is used, and a message is provided.
test('skip option with message', { skip: 'this is skipped' }, (t) => {
  // This code is never executed.
});

test('skip() method', (t) => {
  // Make sure to return here as well if the test contains additional logic.
  t.skip();
});

test('skip() method with message', (t) => {
  // Make sure to return here as well if the test contains additional logic.
  t.skip('this is skipped');
});
```

## `describe`/`it` syntax

Running tests can also be done using `describe` to declare a suite
and `it` to declare a test.
A suite is used to organize and group related tests together.
`it` is an alias for `test`, except there is no test context passed,
since nesting is done using suites.

```js
describe('A thing', () => {
  it('should work', () => {
    assert.strictEqual(1, 1);
  });

  it('should be ok', () => {
    assert.strictEqual(2, 2);
  });

  describe('a nested thing', () => {
    it('should work', () => {
      assert.strictEqual(3, 3);
    });
  });
});
```

`describe` and `it` are imported from the `node:test` module.

```mjs
import { describe, it } from 'node:test';
```

```cjs
const { describe, it } = require('node:test');
```

### `only` tests

If Node.js is started with the [`--test-only`][] command-line option, it is
possible to skip all top level tests except for a selected subset by passing
the `only` option to the tests that should be run. When a test with the `only`
option set is run, all subtests are also run. The test context's `runOnly()`
method can be used to implement the same behavior at the subtest level.

```js
// Assume Node.js is run with the --test-only command-line option.
// The 'only' option is set, so this test is run.
test('this test is run', { only: true }, async (t) => {
  // Within this test, all subtests are run by default.
  await t.test('running subtest');

  // The test context can be updated to run subtests with the 'only' option.
  t.runOnly(true);
  await t.test('this subtest is now skipped');
  await t.test('this subtest is run', { only: true });

  // Switch the context back to execute all tests.
  t.runOnly(false);
  await t.test('this subtest is now run');

  // Explicitly do not run these tests.
  await t.test('skipped subtest 3', { only: false });
  await t.test('skipped subtest 4', { skip: true });
});

// The 'only' option is not set, so this test is skipped.
test('this test is not run', () => {
  // This code is not run.
  throw new Error('fail');
});
```

## Extraneous asynchronous activity

Once a test function finishes executing, the TAP results are output as quickly
as possible while maintaining the order of the tests. However, it is possible
for the test function to generate asynchronous activity that outlives the test
itself. The test runner handles this type of activity, but does not delay the
reporting of test results in order to accommodate it.

In the following example, a test completes with two `setImmediate()`
operations still outstanding. The first `setImmediate()` attempts to create a
new subtest. Because the parent test has already finished and output its
results, the new subtest is immediately marked as failed, and reported in the
top level of the file's TAP output.

The second `setImmediate()` creates an `uncaughtException` event.
`uncaughtException` and `unhandledRejection` events originating from a completed
test are handled by the `test` module and reported as diagnostic warnings in
the top level of the file's TAP output.

```js
test('a test that creates asynchronous activity', (t) => {
  setImmediate(() => {
    t.test('subtest that is created too late', (t) => {
      throw new Error('error1');
    });
  });

  setImmediate(() => {
    throw new Error('error2');
  });

  // The test finishes after this line.
});
```

## Running tests from the command line

The Node.js test runner can be invoked from the command line by passing the
[`--test`][] flag:

```bash
node --test
```

By default, Node.js will recursively search the current directory for
JavaScript source files matching a specific naming convention. Matching files
are executed as test files. More information on the expected test file naming
convention and behavior can be found in the [test runner execution model][]
section.

Alternatively, one or more paths can be provided as the final argument(s) to
the Node.js command, as shown below.

```bash
node --test test1.js test2.mjs custom_test_dir/
```

In this example, the test runner will execute the files `test1.js` and
`test2.mjs`. The test runner will also recursively search the
`custom_test_dir/` directory for test files to execute.

### Test runner execution model

When searching for test files to execute, the test runner behaves as follows:

* Any files explicitly provided by the user are executed.
* If the user did not explicitly specify any paths, the current working
  directory is recursively searched for files as specified in the following
  steps.
* `node_modules` directories are skipped unless explicitly provided by the
  user.
* If a directory named `test` is encountered, the test runner will search it
  recursively for all all `.js`, `.cjs`, and `.mjs` files. All of these files
  are treated as test files, and do not need to match the specific naming
  convention detailed below. This is to accommodate projects that place all of
  their tests in a single `test` directory.
* In all other directories, `.js`, `.cjs`, and `.mjs` files matching the
  following patterns are treated as test files:
  * `^test$` - Files whose basename is the string `'test'`. Examples:
    `test.js`, `test.cjs`, `test.mjs`.
  * `^test-.+` - Files whose basename starts with the string `'test-'`
    followed by one or more characters. Examples: `test-example.js`,
    `test-another-example.mjs`.
  * `.+[\.\-\_]test$` - Files whose basename ends with `.test`, `-test`, or
    `_test`, preceded by one or more characters. Examples: `example.test.js`,
    `example-test.cjs`, `example_test.mjs`.
  * Other file types understood by Node.js such as `.node` and `.json` are not
    automatically executed by the test runner, but are supported if explicitly
    provided on the command line.

Each matching test file is executed in a separate child process. If the child
process finishes with an exit code of 0, the test is considered passing.
Otherwise, the test is considered to be a failure. Test files must be
executable by Node.js, but are not required to use the `node:test` module
internally.

## `run([options])`

<!-- YAML
added: v16.19.0
-->

* `options` {Object} Configuration options for running tests. The following
  properties are supported:
  * `concurrency` {number|boolean} If a number is provided,
    then that many files would run in parallel.
    If truthy, it would run (number of cpu cores - 1)
    files in parallel.
    If falsy, it would only run one file at a time.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `true`.
  * `files`: {Array} An array containing the list of files to run.
    **Default** matching files from [test runner execution model][].
  * `signal` {AbortSignal} Allows aborting an in-progress test execution.
  * `timeout` {number} A number of milliseconds the test execution will
    fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.
  * `inspectPort` {number|Function} Sets inspector port of test child process.
    This can be a number, or a function that takes no arguments and returns a
    number. If a nullish value is provided, each process gets its own port,
    incremented from the primary's `process.debugPort`.
    **Default:** `undefined`.
* Returns: {TapStream}

```js
run({ files: [path.resolve('./tests/test.js')] })
  .pipe(process.stdout);
```

## `test([name][, options][, fn])`

<!-- YAML
added: v16.17.0
changes:
  - version: v16.18.0
    pr-url: https://github.com/nodejs/node/pull/43554
    description: Add a `signal` option.
  - version: v16.17.0
    pr-url: https://github.com/nodejs/node/pull/43505
    description: Add a `timeout` option.
-->

* `name` {string} The name of the test, which is displayed when reporting test
  results. **Default:** The `name` property of `fn`, or `'<anonymous>'` if `fn`
  does not have a name.
* `options` {Object} Configuration options for the test. The following
  properties are supported:
  * `concurrency` {number|boolean} If a number is provided,
    then that many tests would run in parallel.
    If truthy, it would run (number of cpu cores - 1)
    tests in parallel.
    For subtests, it will be `Infinity` tests in parallel.
    If falsy, it would only run one test at a time.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `false`.
  * `only` {boolean} If truthy, and the test context is configured to run
    `only` tests, then this test will be run. Otherwise, the test is skipped.
    **Default:** `false`.
  * `signal` {AbortSignal} Allows aborting an in-progress test.
  * `skip` {boolean|string} If truthy, the test is skipped. If a string is
    provided, that string is displayed in the test results as the reason for
    skipping the test. **Default:** `false`.
  * `todo` {boolean|string} If truthy, the test marked as `TODO`. If a string
    is provided, that string is displayed in the test results as the reason why
    the test is `TODO`. **Default:** `false`.
  * `timeout` {number} A number of milliseconds the test will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.
* `fn` {Function|AsyncFunction} The function under test. The first argument
  to this function is a [`TestContext`][] object. If the test uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* Returns: {Promise} Resolved with `undefined` once the test completes.

The `test()` function is the value imported from the `test` module. Each
invocation of this function results in the creation of a test point in the TAP
output.

The `TestContext` object passed to the `fn` argument can be used to perform
actions related to the current test. Examples include skipping the test, adding
additional TAP diagnostic information, or creating subtests.

`test()` returns a `Promise` that resolves once the test completes. The return
value can usually be discarded for top level tests. However, the return value
from subtests should be used to prevent the parent test from finishing first
and cancelling the subtest as shown in the following example.

```js
test('top level test', async (t) => {
  // The setTimeout() in the following subtest would cause it to outlive its
  // parent test if 'await' is removed on the next line. Once the parent test
  // completes, it will cancel any outstanding subtests.
  await t.test('longer running subtest', async (t) => {
    return new Promise((resolve, reject) => {
      setTimeout(resolve, 1000);
    });
  });
});
```

The `timeout` option can be used to fail the test if it takes longer than
`timeout` milliseconds to complete. However, it is not a reliable mechanism for
canceling tests because a running test might block the application thread and
thus prevent the scheduled cancellation.

## `describe([name][, options][, fn])`

* `name` {string} The name of the suite, which is displayed when reporting test
  results. **Default:** The `name` property of `fn`, or `'<anonymous>'` if `fn`
  does not have a name.
* `options` {Object} Configuration options for the suite.
  supports the same options as `test([name][, options][, fn])`.
* `fn` {Function|AsyncFunction} The function under suite
  declaring all subtests and subsuites.
  The first argument to this function is a [`SuiteContext`][] object.
  **Default:** A no-op function.
* Returns: `undefined`.

The `describe()` function imported from the `node:test` module. Each
invocation of this function results in the creation of a Subtest
and a test point in the TAP output.
After invocation of top level `describe` functions,
all top level tests and suites will execute.

## `describe.skip([name][, options][, fn])`

Shorthand for skipping a suite, same as [`describe([name], { skip: true }[, fn])`][describe options].

## `describe.todo([name][, options][, fn])`

Shorthand for marking a suite as `TODO`, same as
[`describe([name], { todo: true }[, fn])`][describe options].

## `it([name][, options][, fn])`

* `name` {string} The name of the test, which is displayed when reporting test
  results. **Default:** The `name` property of `fn`, or `'<anonymous>'` if `fn`
  does not have a name.
* `options` {Object} Configuration options for the suite.
  supports the same options as `test([name][, options][, fn])`.
* `fn` {Function|AsyncFunction} The function under test.
  If the test uses callbacks, the callback function is passed as an argument.
  **Default:** A no-op function.
* Returns: `undefined`.

The `it()` function is the value imported from the `node:test` module.
Each invocation of this function results in the creation of a test point in the
TAP output.

## `it.skip([name][, options][, fn])`

Shorthand for skipping a test,
same as [`it([name], { skip: true }[, fn])`][it options].

## `it.todo([name][, options][, fn])`

Shorthand for marking a test as `TODO`,
same as [`it([name], { todo: true }[, fn])`][it options].

## `before([, fn][, options])`

<!-- YAML
added: v16.18.0
-->

* `fn` {Function|AsyncFunction} The hook function.
  If the hook uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* `options` {Object} Configuration options for the hook. The following
  properties are supported:
  * `signal` {AbortSignal} Allows aborting an in-progress hook.
  * `timeout` {number} A number of milliseconds the hook will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.

This function is used to create a hook running before running a suite.

```js
describe('tests', async () => {
  before(() => console.log('about to run some test'));
  it('is a subtest', () => {
    assert.ok('some relevant assertion here');
  });
});
```

## `after([, fn][, options])`

<!-- YAML
added: v16.18.0
-->

* `fn` {Function|AsyncFunction} The hook function.
  If the hook uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* `options` {Object} Configuration options for the hook. The following
  properties are supported:
  * `signal` {AbortSignal} Allows aborting an in-progress hook.
  * `timeout` {number} A number of milliseconds the hook will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.

This function is used to create a hook running after  running a suite.

```js
describe('tests', async () => {
  after(() => console.log('finished running tests'));
  it('is a subtest', () => {
    assert.ok('some relevant assertion here');
  });
});
```

## `beforeEach([, fn][, options])`

<!-- YAML
added: v16.18.0
-->

* `fn` {Function|AsyncFunction} The hook function.
  If the hook uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* `options` {Object} Configuration options for the hook. The following
  properties are supported:
  * `signal` {AbortSignal} Allows aborting an in-progress hook.
  * `timeout` {number} A number of milliseconds the hook will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.

This function is used to create a hook running
before each subtest of the current suite.

```js
describe('tests', async () => {
  beforeEach(() => t.diagnostic('about to run a test'));
  it('is a subtest', () => {
    assert.ok('some relevant assertion here');
  });
});
```

## `afterEach([, fn][, options])`

<!-- YAML
added: v16.18.0
-->

* `fn` {Function|AsyncFunction} The hook function.
  If the hook uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* `options` {Object} Configuration options for the hook. The following
  properties are supported:
  * `signal` {AbortSignal} Allows aborting an in-progress hook.
  * `timeout` {number} A number of milliseconds the hook will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.

This function is used to create a hook running
after each subtest of the current test.

```js
describe('tests', async () => {
  afterEach(() => t.diagnostic('about to run a test'));
  it('is a subtest', () => {
    assert.ok('some relevant assertion here');
  });
});
```

## Class: `TapStream`

<!-- YAML
added: v16.19.0
-->

* Extends {ReadableStream}

A successful call to [`run()`][] method will return a new {TapStream}
object, streaming a [TAP][] output
`TapStream` will emit events, in the order of the tests definition

### Event: `'test:diagnostic'`

* `message` {string} The diagnostic message.

Emitted when [`context.diagnostic`][] is called.

### Event: `'test:fail'`

* `data` {Object}
  * `duration` {number} The test duration.
  * `error` {Error} The failure casing test to fail.
  * `name` {string} The test name.
  * `testNumber` {number} The ordinal number of the test.
  * `todo` {string|undefined} Present if [`context.todo`][] is called
  * `skip` {string|undefined} Present if [`context.skip`][] is called

Emitted when a test fails.

### Event: `'test:pass'`

* `data` {Object}
  * `duration` {number} The test duration.
  * `name` {string} The test name.
  * `testNumber` {number} The ordinal number of the test.
  * `todo` {string|undefined} Present if [`context.todo`][] is called
  * `skip` {string|undefined} Present if [`context.skip`][] is called

Emitted when a test passes.

## Class: `TestContext`

<!-- YAML
added: v16.17.0
-->

An instance of `TestContext` is passed to each test function in order to
interact with the test runner. However, the `TestContext` constructor is not
exposed as part of the API.

### `context.beforeEach([, fn][, options])`

<!-- YAML
added: v16.18.0
-->

* `fn` {Function|AsyncFunction} The hook function. The first argument
  to this function is a [`TestContext`][] object. If the hook uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* `options` {Object} Configuration options for the hook. The following
  properties are supported:
  * `signal` {AbortSignal} Allows aborting an in-progress hook.
  * `timeout` {number} A number of milliseconds the hook will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.

This function is used to create a hook running
before each subtest of the current test.

```js
test('top level test', async (t) => {
  t.beforeEach((t) => t.diagnostic(`about to run ${t.name}`));
  await t.test(
    'This is a subtest',
    (t) => {
      assert.ok('some relevant assertion here');
    }
  );
});
```

### `context.afterEach([, fn][, options])`

<!-- YAML
added: v16.18.0
-->

* `fn` {Function|AsyncFunction} The hook function. The first argument
  to this function is a [`TestContext`][] object. If the hook uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* `options` {Object} Configuration options for the hook. The following
  properties are supported:
  * `signal` {AbortSignal} Allows aborting an in-progress hook.
  * `timeout` {number} A number of milliseconds the hook will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.

This function is used to create a hook running
after each subtest of the current test.

```js
test('top level test', async (t) => {
  t.afterEach((t) => t.diagnostic(`finished running ${t.name}`));
  await t.test(
    'This is a subtest',
    (t) => {
      assert.ok('some relevant assertion here');
    }
  );
});
```

### `context.diagnostic(message)`

<!-- YAML
added: v16.17.0
-->

* `message` {string} Message to be displayed as a TAP diagnostic.

This function is used to write TAP diagnostics to the output. Any diagnostic
information is included at the end of the test's results. This function does
not return a value.

```js
test('top level test', (t) => {
  t.diagnostic('A diagnostic message');
});
```

### `context.name`

<!-- YAML
added: v16.18.0
-->

The name of the test.

### `context.runOnly(shouldRunOnlyTests)`

<!-- YAML
added: v16.17.0
-->

* `shouldRunOnlyTests` {boolean} Whether or not to run `only` tests.

If `shouldRunOnlyTests` is truthy, the test context will only run tests that
have the `only` option set. Otherwise, all tests are run. If Node.js was not
started with the [`--test-only`][] command-line option, this function is a
no-op.

```js
test('top level test', (t) => {
  // The test context can be set to run subtests with the 'only' option.
  t.runOnly(true);
  return Promise.all([
    t.test('this subtest is now skipped'),
    t.test('this subtest is run', { only: true }),
  ]);
});
```

### `context.signal`

<!-- YAML
added: v16.17.0
-->

* {AbortSignal} Can be used to abort test subtasks when the test has been
  aborted.

```js
test('top level test', async (t) => {
  await fetch('some/uri', { signal: t.signal });
});
```

### `context.skip([message])`

<!-- YAML
added: v16.17.0
-->

* `message` {string} Optional skip message to be displayed in TAP output.

This function causes the test's output to indicate the test as skipped. If
`message` is provided, it is included in the TAP output. Calling `skip()` does
not terminate execution of the test function. This function does not return a
value.

```js
test('top level test', (t) => {
  // Make sure to return here as well if the test contains additional logic.
  t.skip('this is skipped');
});
```

### `context.todo([message])`

<!-- YAML
added: v16.17.0
-->

* `message` {string} Optional `TODO` message to be displayed in TAP output.

This function adds a `TODO` directive to the test's output. If `message` is
provided, it is included in the TAP output. Calling `todo()` does not terminate
execution of the test function. This function does not return a value.

```js
test('top level test', (t) => {
  // This test is marked as `TODO`
  t.todo('this is a todo');
});
```

### `context.test([name][, options][, fn])`

<!-- YAML
added: v16.17.0
changes:
  - version: v16.18.0
    pr-url: https://github.com/nodejs/node/pull/43554
    description: Add a `signal` option.
  - version: v16.17.0
    pr-url: https://github.com/nodejs/node/pull/43505
    description: Add a `timeout` option.
-->

* `name` {string} The name of the subtest, which is displayed when reporting
  test results. **Default:** The `name` property of `fn`, or `'<anonymous>'` if
  `fn` does not have a name.
* `options` {Object} Configuration options for the subtest. The following
  properties are supported:
  * `concurrency` {number} The number of tests that can be run at the same time.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `1`.
  * `only` {boolean} If truthy, and the test context is configured to run
    `only` tests, then this test will be run. Otherwise, the test is skipped.
    **Default:** `false`.
  * `signal` {AbortSignal} Allows aborting an in-progress test.
  * `skip` {boolean|string} If truthy, the test is skipped. If a string is
    provided, that string is displayed in the test results as the reason for
    skipping the test. **Default:** `false`.
  * `todo` {boolean|string} If truthy, the test marked as `TODO`. If a string
    is provided, that string is displayed in the test results as the reason why
    the test is `TODO`. **Default:** `false`.
  * `timeout` {number} A number of milliseconds the test will fail after.
    If unspecified, subtests inherit this value from their parent.
    **Default:** `Infinity`.
* `fn` {Function|AsyncFunction} The function under test. The first argument
  to this function is a [`TestContext`][] object. If the test uses callbacks,
  the callback function is passed as the second argument. **Default:** A no-op
  function.
* Returns: {Promise} Resolved with `undefined` once the test completes.

This function is used to create subtests under the current test. This function
behaves in the same fashion as the top level [`test()`][] function.

```js
test('top level test', async (t) => {
  await t.test(
    'This is a subtest',
    { only: false, skip: false, concurrency: 1, todo: false },
    (t) => {
      assert.ok('some relevant assertion here');
    }
  );
});
```

## Class: `SuiteContext`

<!-- YAML
added: v16.17.0
-->

An instance of `SuiteContext` is passed to each suite function in order to
interact with the test runner. However, the `SuiteContext` constructor is not
exposed as part of the API.

### `context.name`

<!-- YAML
added: v16.18.0
-->

The name of the suite.

### `context.signal`

<!-- YAML
added: v16.17.0
-->

* {AbortSignal} Can be used to abort test subtasks when the test has been
  aborted.

[TAP]: https://testanything.org/
[`--test-only`]: cli.md#--test-only
[`--test`]: cli.md#--test
[`SuiteContext`]: #class-suitecontext
[`TestContext`]: #class-testcontext
[`context.diagnostic`]: #contextdiagnosticmessage
[`context.skip`]: #contextskipmessage
[`context.todo`]: #contexttodomessage
[`run()`]: #runoptions
[`test()`]: #testname-options-fn
[describe options]: #describename-options-fn
[it options]: #testname-options-fn
[test runner execution model]: #test-runner-execution-model
