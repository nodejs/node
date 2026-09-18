'use strict';
const common = require('../common');

// The working set peaks around 4 GiB, far beyond a 32-bit heap.
common.skipIf32Bits();

if (!common.hasIntl)
  common.skip('missing Intl');

// Peak RSS is around 4 GiB in the BOM case: a 1 GiB input, an ICU target
// buffer of String::kMaxLength + 1 UChars (about 1 GiB), and the decoded
// string with its transient copy.
if (require('os').totalmem() < 8 * 2 ** 30)
  common.skip('less than 8 GiB of total memory');

const assert = require('assert');
const kStringMaxLength = require('buffer').constants.MAX_STRING_LENGTH;

function allocOrSkip(bytes) {
  try {
    return Buffer.allocUnsafe(bytes);
  } catch (e) {
    if (
      e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
      /Array buffer allocation failed/.test(e.message)
    ) {
      common.skip('insufficient space for Buffer.allocUnsafe');
    }

    throw e;
  }
}

function assertThrowsTooLong(fn) {
  assert.throws(fn, (e) => {
    assert.strictEqual(e.code, 'ERR_STRING_TOO_LONG');
    return true;
  });
}

{
  // One UTF-16 code unit beyond the maximum string length. The target
  // buffer is clamped to kStringMaxLength + 1 UChars, so the result fits
  // the buffer exactly, there is no leading BOM to strip, and
  // StringBytes::Encode() rejects the oversized result. That must surface
  // as ERR_STRING_TOO_LONG rather than ERR_ENCODING_INVALID_ENCODED_DATA.
  const size = 2 * kStringMaxLength + 2;
  const input = allocOrSkip(size);
  input.fill(0x20);
  assertThrowsTooLong(() => new TextDecoder('utf-16le').decode(input));
}

{
  // A BOM plus exactly kStringMaxLength characters: the decode fills the
  // clamped buffer exactly, the BOM is stripped, and the result is the
  // longest possible string. This must succeed; it is why the clamp is
  // kStringMaxLength + 1 and not kStringMaxLength.
  const size = 2 * kStringMaxLength + 2;
  const input = allocOrSkip(size);
  input.fill(0x20);
  input[0] = 0xFF;
  input[1] = 0xFE;
  const result = new TextDecoder('utf-16le').decode(input);
  assert.strictEqual(result.length, kStringMaxLength);
  assert.strictEqual(result.charCodeAt(0), 0x2020);
}

{
  // Two characters beyond the buffer through a min_char_size() == 1
  // encoding: pure-ASCII input of kStringMaxLength + 2 bytes wants
  // kStringMaxLength + 2 UChars, overflows the clamped target buffer
  // inside ICU, and the U_BUFFER_OVERFLOW_ERROR path must report
  // ERR_STRING_TOO_LONG rather than ERR_ENCODING_INVALID_ENCODED_DATA.
  // gb18030 needs full-icu; skip the case silently on small-icu builds
  // (the utf-16le cases above ran).
  let decoder;
  try {
    decoder = new TextDecoder('gb18030');
  } catch (e) {
    if (e.code !== 'ERR_ENCODING_NOT_SUPPORTED')
      throw e;
  }

  if (decoder !== undefined) {
    const input = allocOrSkip(kStringMaxLength + 2);
    input.fill(0x41);
    assertThrowsTooLong(() => decoder.decode(input));
  }
}
