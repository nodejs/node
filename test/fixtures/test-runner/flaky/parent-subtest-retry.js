'use strict';
// A flaky parent whose only failure comes from a SUBTEST must be retried: the
// child fails on attempts 1-2 and passes on attempt 3, so the parent must keep
// retrying and ultimately pass. The state file records the attempt count.
const { test } = require('node:test');
const assert = require('node:assert');
const { writeFileSync } = require('node:fs');

const stateFile = process.env.FLAKY_STATE;
let attempt = 0;

test('parent retries when subtest fails', { flaky: 3 }, async (t) => {
  attempt++;
  writeFileSync(stateFile, String(attempt));
  await t.test('child', () => {
    if (attempt < 3) {
      assert.fail(`child fails on attempt ${attempt}`);
    }
  });
});
