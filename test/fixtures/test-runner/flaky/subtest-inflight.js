'use strict';
// A flaky parent that creates an unawaited (in-flight) subtest and then throws
// must not corrupt the run: the in-flight subtest from a discarded attempt must
// be settled before the retry resets per-attempt state. The parent passes on
// the final attempt; the harness asserts the run completes with the correct
// final result (the intermediate-attempt subtest leak is a known limitation).
const { test } = require('node:test');

let attempt = 0;

test('flaky parent with in-flight subtest', { flaky: 3 }, (t) => {
  attempt++;
  t.test('child', () => new Promise((resolve) => setTimeout(resolve, 20)));
  if (attempt < 3) {
    throw new Error('retry');
  }
});
