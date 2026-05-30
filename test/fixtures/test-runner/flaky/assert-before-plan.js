'use strict';
const { test } = require('node:test');
test('assert accessed before plan (non-flaky)', (t) => {
  const a = t.assert;
  t.plan(0);
  a.ok(true);
});
