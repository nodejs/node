// Flags: --test-reporter=tap
'use strict';
const { test } = require('node:test');
const assert = require('node:assert');

// Test Date vs Date comparison
test('date vs date comparison', () => {
  const expected = new Date('2023-01-01T12:00:00.000Z');
  const actual = new Date('2023-01-01T13:00:00.000Z');
  assert.deepStrictEqual(actual, expected);
});

// Test Date vs String comparison
test('date vs string comparison', () => {
  const expected = '2023-01-01T12:00:00.000Z';
  const actual = new Date('2023-01-01T13:00:00.000Z');
  assert.deepStrictEqual(actual, expected);
});

// Test nested Date objects
test('nested date objects', () => {
  const expected = {
    timestamp: new Date('2023-01-01T12:00:00.000Z'),
    message: 'test'
  };
  const actual = {
    timestamp: new Date('2023-01-01T13:00:00.000Z'),
    message: 'test'
  };
  assert.deepStrictEqual(actual, expected);
}); 