// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {toLength} = require('internal/util');

assert.strictEqual(toLength(+0), 0);
assert.strictEqual(toLength(-0), 0);
assert.strictEqual(toLength(NaN), 0);
assert.strictEqual(toLength(Math.pow(2, 60)), 9007199254740991);
assert.strictEqual(toLength(Infinity), 9007199254740991);
assert.strictEqual(toLength(-Infinity), 0);

assert.strictEqual(toLength(0.1), 0);
assert.strictEqual(toLength(-0.1), 0);
assert.strictEqual(toLength(0.5), 0);
assert.strictEqual(toLength(-0.5), 0);
assert.strictEqual(toLength(0.9), 0);
assert.strictEqual(toLength(-0.9), 0);
