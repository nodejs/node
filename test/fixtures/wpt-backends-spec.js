'use strict';

// Driven through both WPT runner backends by
// test/parallel/test-common-wpt-backends.js. Between them these produce every
// kind of message the two backends have to agree on.

test(() => {}, 'passes');

test(() => {
  assert_true(false, 'deliberate failure');
}, 'fails');

test(() => {}, 'skipped by name');

test(() => {}, 'skipped by pattern');

promise_test(async () => {}, 'passes asynchronously');

if (globalThis.WPT_BACKENDS_THROW) {
  // Thrown once results have already been reported, which is when the backends
  // stop sharing a mechanism: a worker thread surfaces this through its
  // 'error' event, a process has to report it before exiting.
  throw new Error('deliberate uncaught error');
}
