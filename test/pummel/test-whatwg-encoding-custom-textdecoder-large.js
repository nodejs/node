'use strict';
const common = require('../common');

// Input large enough that the old 4x target bound exceeded ICU's
// 0x3fffffff UChar limit; also needs more than a 32-bit heap.
common.skipIf32Bits();

if (!common.hasIntl)
  common.skip('missing Intl');

// Peak RSS is around 1.6 GiB: the input, the ICU target buffer, and two
// result strings.
if (require('os').totalmem() < 8 * 2 ** 30)
  common.skip('less than 8 GiB of total memory');

const assert = require('assert');

const size = 2 ** 27;

let input;

try {
  input = Buffer.allocUnsafe(size * 2);
} catch (e) {
  if (
    e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
    /Array buffer allocation failed/.test(e.message)
  ) {
    common.skip('insufficient space for Buffer.allocUnsafe');
  }

  throw e;
}

// Non-uniform repeating pattern of A, a U+1F600 surrogate pair and 中,
// written as explicit little-endian bytes so the input is identical on
// big-endian hosts. Corrupted or misplaced output cannot match it.
input.fill(Buffer.from([0x41, 0x00, 0x3D, 0xD8, 0x00, 0xDE, 0x2D, 0x4E]));

const decoder = new TextDecoder('utf-16le');

// 2 ** 27 UTF-16 code units used to fail with
// ERR_ENCODING_INVALID_ENCODED_DATA because the target buffer request
// exceeded ICU's internal targetLimit validation.
// Refs: https://github.com/nodejs/node/issues/47645
const result = decoder.decode(input);
assert.strictEqual(result.length, size);
assert.strictEqual(result[0], 'A');
assert.strictEqual(result[1], '\uD83D');
assert.strictEqual(result[2], '\uDE00');
assert.strictEqual(result[size / 2], 'A');
assert.strictEqual(result[size - 1], '中');

// Guard against over-correction: one code unit below the failure boundary
// decodes at HEAD too and must keep doing so. The truncation removes the
// trailing 中, so it does not split a surrogate pair.
assert.strictEqual(decoder.decode(input.subarray(0, size * 2 - 2)).length,
                   size - 1);

// Streaming with an odd byte split lands mid-code-unit, so one byte stays
// pending in the converter across the chunk boundary. The full content is
// compared against the non-streaming result, so any corruption at the
// boundary fails the test.
const split = 2 ** 26 + 1;
const streamed = decoder.decode(input.subarray(0, split), { stream: true }) +
                 decoder.decode(input.subarray(split));
assert.strictEqual(streamed.length, result.length);
assert.strictEqual(streamed, result);
