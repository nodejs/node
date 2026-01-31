'use strict';
const { test } = require('node:test');
const assert = require('node:assert');
const { getValue } = require('./source.js');

// Only call with true, so the false branch is "uncovered" but ignored
test('getValue returns truthy for true', () => {
  assert.strictEqual(getValue(true), 'truthy');
});
