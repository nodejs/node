'use strict';
const { test } = require('node:test');
const assert = require('assert');

test('failing test 1', () => {
  assert.strictEqual(1, 2, 'This test should fail');
});

test('failing test 2', () => {
  assert.strictEqual(3, 4, 'This test fails as well');
});
