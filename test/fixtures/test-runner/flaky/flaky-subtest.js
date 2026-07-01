'use strict';
const { it } = require('node:test');
let n = 0;
it.flaky('parent retries, subtest only on first attempt', { flaky: 3 }, async (t) => {
  n++;
  if (n < 2) {
    await t.test('child', () => {});       // subtest passes on attempt 1
    throw new Error('body fails on attempt 1'); // body throws -> parent retries
  }
  // attempt 2: no subtest, no throw -> passes
});
