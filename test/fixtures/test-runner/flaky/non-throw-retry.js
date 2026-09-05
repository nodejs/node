'use strict';
const { test } = require('node:test');
let attempts = 0;
test('non-throw failure retried', { flaky: 4 }, (t) => {
  attempts++;
  t.plan(1);
  if (attempts < 2) {
    t.assert.ok(false); // non-throwing assertion failure
  } else {
    t.assert.ok(true);
  }
});
