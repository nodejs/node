'use strict';
const common = require('../common');

// The working set peaks around 3 GiB, far beyond a 32-bit heap.
common.skipIf32Bits();

if (!common.hasIntl)
  common.skip('missing Intl');

// Peak RSS is around 3 GiB in the StringBytes::Encode() case: a 1 GiB
// input, an ICU target buffer of String::kMaxLength + 1 UChars (about
// 1 GiB), and the copy of it that Encode() makes before V8 rejects the
// string.
if (require('os').totalmem() < 8 * 2 ** 30)
  common.skip('less than 8 GiB of total memory');

const assert = require('assert');
const kStringMaxLength = require('buffer').constants.MAX_STRING_LENGTH;

// A too-long result in stream mode resets the decoder, like a decoding
// error does, so the next call starts fresh.
// Refs: https://github.com/nodejs/node/pull/67610

let input;

try {
  input = Buffer.allocUnsafe(2 * kStringMaxLength + 4);
} catch (e) {
  if (
    e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
    /Array buffer allocation failed/.test(e.message)
  ) {
    common.skip('insufficient space for Buffer.allocUnsafe');
  }

  throw e;
}

input.fill(0x20);

function decodeAfterTooLong(first, next) {
  const decoder = new TextDecoder('utf-16le');
  assert.throws(() => decoder.decode(first, { stream: true }),
                { code: 'ERR_STRING_TOO_LONG' });
  return decoder.decode(new Uint8Array(next));
}

// kStringMaxLength + 1 code units: the result fits the target buffer but
// not a string, so StringBytes::Encode() fails after the BOM has been
// marked as seen.
assert.strictEqual(
  decodeAfterTooLong(input.subarray(0, -2), [0xFF, 0xFE, 0x42, 0x00]),
  'B');

// kStringMaxLength + 2 code units ending in a surrogate pair: the result
// overflows the target buffer between the two halves of the pair. ICU
// keeps the trail surrogate in its overflow buffer and would emit it at
// the start of the next call.
input.set([0x3D, 0xD8, 0x00, 0xDE], input.length - 4);
assert.strictEqual(decodeAfterTooLong(input, [0x41, 0x00]), 'A');
