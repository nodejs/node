'use strict';

const assert = require('node:assert');
const test = require('node:test');
const { getValue } = require('./coverage-ignored-branch');

test('returns truthy', () => {
  assert.strictEqual(getValue(true), 'truthy');
});
