'use strict';
// Only the subtest fails, so `after` must stay for the final attempt instead
// of tearing state down between retries.
const { appendFileSync } = require('node:fs');
const { test } = require('node:test');
const assert = require('node:assert');

const state = process.env.FLAKY_STATE;
let attempt = 0;

test('after stays for the final attempt on subtest failure', { flaky: 3 }, async (t) => {
  attempt++;
  appendFileSync(state, `body${attempt};`);
  const registeredAt = attempt;
  t.after(() => appendFileSync(state, `after${registeredAt};`));
  await t.test('child', () => {
    if (attempt < 3) {
      assert.fail(`child fails on attempt ${attempt}`);
    }
  });
});
