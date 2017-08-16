/* eslint-disable strict */
require('../common');
const assert = require('assert');

const zlib = require('zlib');

assert.strictEqual(zlib.constants.Z_OK, 0, 'Z_OK should be 0');
zlib.constants.Z_OK = 1;
assert.strictEqual(zlib.constants.Z_OK, 0, 'Z_OK should be 0');

assert.strictEqual(zlib.codes.Z_OK, 0, 'Z_OK should be 0');
zlib.codes.Z_OK = 1;
assert.strictEqual(zlib.codes.Z_OK, 0, 'zlib.codes.Z_OK should be 0');
zlib.codes = { Z_OK: 1 };
assert.strictEqual(zlib.codes.Z_OK, 0, 'zlib.codes.Z_OK should be 0');

assert.ok(Object.isFrozen(zlib.codes), 'zlib.codes should be frozen');
