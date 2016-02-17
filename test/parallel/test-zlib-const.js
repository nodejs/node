/* eslint-disable strict */
require('../common');
var assert = require('assert');

var zlib = require('zlib');

assert.equal(zlib.Z_OK, 0, 'Z_OK should be 0');
zlib.Z_OK = 1;
assert.equal(zlib.Z_OK, 0, 'Z_OK should be 0');

assert.equal(zlib.codes.Z_OK, 0, 'Z_OK should be 0');
zlib.codes.Z_OK = 1;
assert.equal(zlib.codes.Z_OK, 0, 'zlib.codes.Z_OK should be 0');
zlib.codes = {Z_OK: 1};
assert.equal(zlib.codes.Z_OK, 0, 'zlib.codes.Z_OK should be 0');

assert.ok(Object.isFrozen(zlib.codes), 'zlib.codes should be frozen');
