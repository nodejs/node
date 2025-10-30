'use strict';

require('../common');
const assert = require('assert');
const { mock } = require('node:test');

console.log('Testing mock.property() with process.env...');

const hadOriginal = Object.prototype.hasOwnProperty.call(process.env, 'TEST_ENV');
const original = process.env.TEST_ENV;


const tracker = mock.property(process.env, 'TEST_ENV', 'mocked');

assert.strictEqual(process.env.TEST_ENV, 'mocked');
console.log('Mocked process.env.TEST_ENV successfully');

tracker.restore();

if (hadOriginal) {
  assert.strictEqual(process.env.TEST_ENV, original);
} else {
  assert.ok(!('TEST_ENV' in process.env));
}

console.log('Restored original process.env.TEST_ENV');
