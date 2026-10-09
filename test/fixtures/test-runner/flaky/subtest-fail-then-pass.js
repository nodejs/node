'use strict';
// The intermediate child failure must not reach the harness counters: the
// retried-and-passing run must exit 0 with `# fail 0`.
const { test } = require('node:test');
const assert = require('node:assert');

let attempt = 0;

test('parent passes after subtest retry', { flaky: 3 }, async (t) => {
  attempt++;
  await t.test('child', () => {
    if (attempt < 2) {
      assert.fail(`child fails on attempt ${attempt}`);
    }
  });
});
