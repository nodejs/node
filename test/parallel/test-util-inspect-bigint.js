'use strict';

// Flags: --harmony-bigint

require('../common');
const assert = require('assert');

const { inspect } = require('util');

assert.strictEqual(inspect(1n), '1n');
assert.strictEqual(inspect(Object(-1n)), '[BigInt: -1n]');
assert.strictEqual(inspect(Object(13n)), '[BigInt: 13n]');
assert.strictEqual(inspect(new BigInt64Array([0n])), 'BigInt64Array [ 0n ]');
assert.strictEqual(inspect(new BigUint64Array([0n])), 'BigUint64Array [ 0n ]');
