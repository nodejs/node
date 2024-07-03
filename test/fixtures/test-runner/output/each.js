'use strict';

require('../../../common');
const assert = require('assert');
const test = require('node:test');

test.each([
  [1, 1, 2],
  [1, 2, 3],
  [2, 1, 3],
  [2, 2, 4],
], '%d + %d = %d', (a, b, expected) => {
  assert.strictEqual(a + b, expected);
});

test.suite('Does it work with suite?', () => {
  test.each([
    [1, 1, 2],
    [1, 2, 3],
    [2, 1, 3],
    [2, 2, 4],
  ], '%d + %d = %d', (a, b, expected) => {
    assert.strictEqual(a + b, expected);
  });
});
