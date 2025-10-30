'use strict';

// Flags: --expose-internals
require('../common');
const assert = require('assert');

// This test verifies mocking and restoring process.env.TEST_ENV

const hadOriginal = Object.prototype.hasOwnProperty.call(process.env, 'TEST_ENV');
const original = process.env.TEST_ENV;

// Mock environment variable
process.env.TEST_ENV = 'mocked';
assert.strictEqual(process.env.TEST_ENV, 'mocked');
console.log('Mocked process.env.TEST_ENV successfully');

// Restore original variable
if (hadOriginal) {
  process.env.TEST_ENV = original;
} else {
  delete process.env.TEST_ENV;
}

//  Validation
if (hadOriginal) {
  assert.strictEqual(process.env.TEST_ENV, original);
} else {
  assert.ok(!('TEST_ENV' in process.env));
}

console.log('Restored original process.env.TEST_ENV');

