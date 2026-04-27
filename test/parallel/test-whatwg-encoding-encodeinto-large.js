'use strict';

const common = require('../common');

common.skipIf32Bits();

const assert = require('assert');

const encoder = new TextEncoder();
const source = 'a\xFF\u6211\u{1D452}';
const expected = encoder.encode(source);

let dest;

try {
  dest = new Uint8Array(2 ** 31);
} catch {
  common.skip('insufficient space for Uint8Array allocation');
}

const result = encoder.encodeInto(source, dest);
assert.deepStrictEqual(result, {
  read: source.length,
  written: expected.length,
});
assert.deepStrictEqual(dest.subarray(0, expected.length), expected);
