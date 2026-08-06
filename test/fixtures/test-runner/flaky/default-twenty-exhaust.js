'use strict';
const { test } = require('node:test');
const assert = require('node:assert');
let a = 0;
test('needs 22 attempts', { flaky: true }, () => {
  a++;
  assert.strictEqual(a, 22); // 21 retries needed, but default is only 20
});
