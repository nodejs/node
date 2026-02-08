'use strict';

const assert = require('node:assert');
const { test } = require('node:test');

class ExtendedArray extends Array {}

test('assertion error preserves prototype name', () => {
  assert.deepStrictEqual(new ExtendedArray('hello'), ['hello']);
});
