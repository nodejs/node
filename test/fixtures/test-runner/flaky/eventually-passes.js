'use strict';
const { test } = require('node:test');
const assert = require('node:assert');
let attempts = 0;
test('passes on third attempt', { flaky: 5 }, (t) => {
  attempts++;
  t.diagnostic('attempt-' + attempts);
  assert.strictEqual(attempts, 3, 'still flaky on attempt ' + attempts);
});
