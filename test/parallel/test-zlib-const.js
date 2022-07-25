/* eslint-disable strict */
require('../common');
const assert = require('assert');

const zlib = require('zlib');

assert.strictEqual(zlib.constants.Z_OK, 0,
                   [
                     'Expected Z_OK to be 0;',
                     `got ${zlib.constants.Z_OK}`,
                   ].join(' '));
zlib.constants.Z_OK = 1;
assert.strictEqual(zlib.constants.Z_OK, 0,
                   [
                     'Z_OK should be immutable.',
                     `Expected to get 0, got ${zlib.constants.Z_OK}`,
                   ].join(' '));

assert.strictEqual(zlib.codes.Z_OK, 0,
                   `Expected Z_OK to be 0; got ${zlib.codes.Z_OK}`);
zlib.codes.Z_OK = 1;
assert.strictEqual(zlib.codes.Z_OK, 0,
                   [
                     'Z_OK should be immutable.',
                     `Expected to get 0, got ${zlib.codes.Z_OK}`,
                   ].join(' '));
zlib.codes = { Z_OK: 1 };
assert.strictEqual(zlib.codes.Z_OK, 0,
                   [
                     'Z_OK should be immutable.',
                     `Expected to get 0, got ${zlib.codes.Z_OK}`,
                   ].join(' '));

assert.ok(Object.isFrozen(zlib.codes),
          [
            'Expected zlib.codes to be frozen, but Object.isFrozen',
            `returned ${Object.isFrozen(zlib.codes)}`,
          ].join(' '));
