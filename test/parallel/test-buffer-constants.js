'use strict';
require('../common');
const assert = require('assert');

const { MAX_LENGTH, MAX_STRING_LENGTH } = require('buffer').constants;

assert.strictEqual(typeof MAX_LENGTH, 'number');
assert.strictEqual(typeof MAX_STRING_LENGTH, 'number');
assert(MAX_STRING_LENGTH <= MAX_LENGTH);
assert.throws(() => ' '.repeat(MAX_STRING_LENGTH + 1),
              /^RangeError: Invalid string length$/);

assert.doesNotThrow(() => ' '.repeat(MAX_STRING_LENGTH));
