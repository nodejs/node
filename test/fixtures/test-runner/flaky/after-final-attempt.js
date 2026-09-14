'use strict';
// A test-level `after` hook must run once, after the FINAL attempt - not after
// the first failing attempt. The hook records the attempt number at which it
// ran; the harness asserts it ran at the final (passing) attempt.
const { test } = require('node:test');
const assert = require('node:assert');
const { writeFileSync } = require('node:fs');

const stateFile = process.env.FLAKY_STATE;
let attempt = 0;

test('after runs after the final attempt', { flaky: 3 }, (t) => {
  attempt++;
  if (attempt === 1) {
    t.after(() => { writeFileSync(stateFile, String(attempt)); });
  }
  if (attempt < 3) {
    assert.fail('force retry');
  }
});
