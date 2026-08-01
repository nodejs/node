# Worker WPT tests

These are the workers (`Worker`, `SharedWorker`) tests for the
[Web workers chapter of the HTML Standard](https://html.spec.whatwg.org/multipage/workers.html).

See also
[testharness.js API > Web Workers](https://web-platform-tests.org/writing-tests/testharness-api.html#web-workers).

Note that because workers are defined in the HTML Standard, the idlharness.js
tests are in [/html/dom]([/html/dom) instead of here.

## Writing `*.any.js`

The easiest and most recommended way to write tests for workers
is to create .any.js-style tests.

Official doc:
[WPT > File Name Flags > Test Features](https://web-platform-tests.org/writing-tests/file-names.html#test-features).

- Standard `testharness.js`-style can be used (and is enforced).
- The same test can be run on window and many types of workers.
- All glue code are automatically generated.
- No need to care about how to create and communicate with each type of workers,
  thanks to `fetch_tests_from_worker` in `testharness.js`.

Converting existing tests into `.any.js`-style also has benefits:

- Multiple tests can be merged into one.
- Tests written for window can be run on workers
  with a very low development cost.

### How to write tests

If you write `testharness.js`-based tests in `foo.any.js` and
specify types of workers to be tested,
the test can run on any of dedicated, shared and service workers.

See `examples/general.any.js` for example.

Even for testing specific features in a specific type of workers
(e.g. shared worker's `onconnect`), `.any.js`-style tests can be used.

See `examples/onconnect.any.js` for example.

### How to debug tests

Whether each individual test passed or failed,
and its assertion failures (if any) are all reported in the final results.

`console.log()` might not appear in the test results and
thus might not be useful for printf debugging.
For example, in Chromium, this message

- Appears (in stderr) on a window or a dedicated worker, but
- Does NOT appear on a shared worker or a service worker.

### How it works

`.any.js`-style tests use
`fetch_tests_from_worker` functionality of `testharness.js`.

The WPT test server generates necessary glue code
(including generated Document HTML and worker top-level scripts).
See
[serve.py](https://github.com/web-platform-tests/wpt/blob/master/tools/serve/serve.py)
for the actual glue code.

Note that `.any.js` file is not the worker top-level script,
and currently we cannot set response headers to the worker top-level script,
e.g. to set Referrer Policy of the workers.

## Writing `*.worker.js`

Similar to `.any.js`, you can also write `.worker.js`
for tests only for dedicated workers.
Almost the same as `.any.js`, except for the things listed below.

Official doc:
[WPT > File Name Flags > Test Features](https://web-platform-tests.org/writing-tests/file-names.html#test-features).

### How to write tests

You have to write two things manually (which is generated in `.any.js` tests):

- `importScripts("/resources/testharness.js");` at the beginning.
- `done();` at the bottom.

Note: Even if you write `async_test()` or `promise_test()`,
this global `done()` is always needed
(this is different from async_test's `done()`)
for dedicated workers and shared workers.
See official doc:
[testharness.js API > Determining when all tests are complete](https://web-platform-tests.org/writing-tests/testharness-api.html#determining-when-all-tests-are-complete).

See `examples/general.worker.js` for example.

### How it works

`.worker.js`-style tests also use
`fetch_tests_from_worker` functionality of `testharness.js`.

The WPT test server generates glue code in Document HTML-side,
but not for worker top-level scripts.
This is why you have to manually write `importScripts()` etc.
See
[serve.py](https://github.com/web-platform-tests/wpt/blob/master/tools/serve/serve.py)
for the actual glue code.

Unlike `*.any.js` cases, the `*.worker.js` is the worker top-level script.

## Using `fetch_tests_from_worker`

If you need more flexibility,
writing tests using `fetch_tests_from_worker` is the way to go.
For example, when

- Additional processing is needed on the parent Document.
- Workers should be created in a specific way.
- You are writing non-WPT tests using `testharness.js`.

You have to write the main HTMLs and the worker scripts,
but most of the glue code needed for running tests on workers
are provided by `fetch_tests_from_worker`.

### How to write tests

See

- `examples/fetch_tests_from_worker.html` and
  `examples/fetch_tests_from_worker.js`.

## Writing the whole tests manually

If `fetch_tests_from_worker` isn't suitable for your specific case
(which should be rare but might be still possible),
you have to write the whole tests,
including the main Document HTML, worker scripts,
and message passing code between them.

TODO: Supply the templates for writing this kind of tests.
