'use strict';

const test = require('node:test');
const assert = require('node:assert');

function getValue(condition) {
  if (condition) {
    return 'truthy';
  }
  /* node:coverage ignore next */
  return 'falsy';
}

test('returns truthy', () => {
  assert.strictEqual(getValue(true), 'truthy');
});
