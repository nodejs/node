'use strict';
const { test } = require('node:test');
const assert = require('node:assert');
let a = 0;
test('passes on attempt 21', { flaky: true }, () => {
  a++;
  assert.strictEqual(a, 21);
});
