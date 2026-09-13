'use strict';

// This tests that TextEncoder.encodeInto() does not underestimate how many
// bytes a code point needs when computing how much of the source string fits
// into the destination.

require('../common');
const assert = require('assert');

// Long enough to bypass the small-string fast path (kSmallStringThreshold = 32
// in src/encoding_binding.cc) and exercise the chunked encoding logic.
const encoder = new TextEncoder();

{
  // Code points in [0x80, 0x800) are 2 bytes in UTF-8; treating them as 3
  // bytes causes encodeInto() to reject input that would actually fit.
  const text = 'Ѐ'.repeat(33);
  const result = encoder.encodeInto(text, new Uint8Array(2));
  assert.strictEqual(result.read, 1);
  assert.strictEqual(result.written, 2);
}

{
  // A one-byte (Latin1) source string takes a different internal path than
  // a two-byte (UTF-16) one. Bytes >= 0x80 must be treated as unsigned there
  // too, or they get sign-extended into a bogus, oversized code point.
  const text = 'é'.repeat(33);
  const result = encoder.encodeInto(text, new Uint8Array(2));
  assert.strictEqual(result.read, 1);
  assert.strictEqual(result.written, 2);

  // Appending a two-byte character forces the whole string to be stored as
  // UTF-16 internally, which must not change how the Latin1-only prefix
  // encodes.
  const withTrailingChar = encoder.encodeInto(
    text + '☺', new Uint8Array(2));
  assert.strictEqual(withTrailingChar.read, result.read);
  assert.strictEqual(withTrailingChar.written, result.written);
}
