'use strict';
// The body re-registers its afterEach on every attempt; the duplicates from
// discarded attempts must not run for later attempts' subtests.
const { appendFileSync } = require('node:fs');
const { test } = require('node:test');
const assert = require('node:assert');

const state = process.env.FLAKY_STATE;
let attempt = 0;

test('per-attempt hooks do not accumulate', { flaky: 2 }, async (t) => {
  attempt++;
  const registeredAt = attempt;
  t.afterEach(() => appendFileSync(state, `ae${registeredAt};`));
  await t.test('child', () => {
    if (attempt === 1) {
      assert.fail('child fails on the first attempt');
    }
  });
});
