// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {toInteger} = require('internal/util');

assert.strictEqual(toInteger(+0), 0);
assert.strictEqual(toInteger(-0), 0);
assert.strictEqual(toInteger(NaN), 0);
assert.strictEqual(toInteger(Infinity), Infinity);
assert.strictEqual(toInteger(-Infinity), -Infinity);

assert.strictEqual(toInteger(0.1), 0);
assert.strictEqual(toInteger(-0.1), 0);
assert.strictEqual(toInteger(0.5), 0);
assert.strictEqual(toInteger(-0.5), 0);
assert.strictEqual(toInteger(0.9), 0);
assert.strictEqual(toInteger(-0.9), 0);
