/* eslint-disable strict */
// Flags: --expose-brotli
require('../common');
const assert = require('assert');

const brotli = require('brotli');

assert.strictEqual(brotli.constants.BROTLI_FALSE, 0,
                   [
                     'Expected BROTLI_FALSE to be 0;',
                     `got ${brotli.constants.BROTLI_FALSE}`
                   ].join(' '));
brotli.constants.BROTLI_FALSE = 1;
assert.strictEqual(brotli.constants.BROTLI_FALSE, 0,
                   [
                     'BROTLI_FALSE should be immutable.',
                     `Expected to get 0, got ${brotli.constants.BROTLI_FALSE}`
                   ].join(' '));

assert.strictEqual(brotli.Compress.codes.BROTLI_FALSE, 0,
                   'Expected BROTLI_FALSE to be 0; got ' +
                    brotli.Compress.codes.BROTLI_FALSE);
brotli.Compress.codes.BROTLI_FALSE = 1;
assert.strictEqual(brotli.Compress.codes.BROTLI_FALSE, 0,
                   [
                     'BROTLI_FALSE should be immutable.',
                     'Expected to get 0, got ' +
                      brotli.Compress.codes.BROTLI_FALSE,
                   ].join(' '));
brotli.Compress.codes = { BROTLI_FALSE: 1 };
assert.strictEqual(brotli.Compress.codes.BROTLI_FALSE, 0,
                   [
                     'BROTLI_FALSE should be immutable.',
                     'Expected to get 0, got ' +
                      brotli.Compress.codes.BROTLI_FALSE,
                   ].join(' '));

assert.ok(Object.isFrozen(brotli.Compress.codes),
          [
            'Expected brotli.codes to be frozen, but Object.isFrozen',
            `returned ${Object.isFrozen(brotli.Compress.codes)}`
          ].join(' '));
