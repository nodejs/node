'use strict';
const assert = require('node:assert');
const { test } = require('node:test');

function sum(n) {
  let result = 0;
  for (let i = 0; i < n; i++) {
    result += i;
  }
  return result;
}

test('sum', () => {
  assert.strictEqual(sum(10), 45);
});
