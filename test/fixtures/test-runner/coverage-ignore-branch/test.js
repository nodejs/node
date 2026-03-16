'use strict';
const { test } = require('node:test');
const assert = require('node:assert');
const { getValue, getMixed } = require('./source.js');

// Only call with true so the false branch is "uncovered" but ignored.
test('getValue returns truthy for true', () => {
  assert.strictEqual(getValue(true), 'truthy');
});

// getMixed has a branch with both ignored and non-ignored uncovered lines.
// The branch should still be reported as uncovered.
test('getMixed returns yes for true', () => {
  assert.strictEqual(getMixed(true), 'yes');
});
