'use strict';

const common = require('../common');

common.skipIf32Bits();

const assert = require('assert');

const encoder = new TextEncoder();
const source = 'a\xFF\u6211\u{1D452}';
const expected = encoder.encode(source);

const size = 2 ** 31 + expected.length;
const offset = expected.length - 1;
let dest;

try {
  dest = new Uint8Array(size);
} catch (e) {
  if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
      /Array buffer allocation failed/.test(e.message)) {
    common.skip('insufficient space for Uint8Array allocation');
  }
  throw e;
}

const large = encoder.encodeInto(source, dest.subarray(offset));
assert.deepStrictEqual(large, {
  read: source.length,
  written: expected.length,
});
assert.deepStrictEqual(dest.slice(offset, offset + expected.length), expected);

const bounded = encoder.encodeInto(source,
                                   dest.subarray(offset,
                                                 offset + expected.length));
assert.deepStrictEqual(bounded, {
  read: source.length,
  written: expected.length,
});
assert.deepStrictEqual(dest.slice(offset, offset + expected.length), expected);
