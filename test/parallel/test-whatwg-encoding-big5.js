'use strict';

// Big5 is decoded by a WHATWG-spec-faithful implementation (lib/internal/encoding/big5.js)
// rather than through ICU, because ICU's own Big5 conversion table includes
// vendor/PUA mappings the WHATWG Encoding Standard does not: byte sequences
// the standard defines as invalid decode to those extra characters instead
// of U+FFFD when routed through ICU.
// Refs: https://github.com/nodejs/node/issues/61041
// Refs: https://github.com/nodejs/node/issues/40091
// Refs: https://encoding.spec.whatwg.org/#big5-decoder

require('../common');
const assert = require('assert');

function codePoints(str) {
  return [...str].map((c) => c.codePointAt(0));
}

for (const label of ['big5', 'Big5', 'BIG5', 'big5-hkscs', 'cn-big5', 'csbig5', 'x-x-big5']) {
  const decoder = new TextDecoder(label);
  assert.strictEqual(decoder.encoding, 'big5');
}

// ASCII round-trips as-is.
{
  const decoder = new TextDecoder('big5');
  assert.strictEqual(decoder.decode(Uint8Array.from([0x41, 0x42, 0x43])), 'ABC');
}

// A valid 2-byte Big5 sequence decodes to the expected code point.
// 0xA4 0x40 is the first Hanzi in the Big5 table: U+4E00 ("一", "one").
{
  const decoder = new TextDecoder('big5');
  assert.strictEqual(decoder.decode(Uint8Array.from([0xa4, 0x40])), '一');
}

// Regression test: an unassigned Big5 pointer must decode to U+FFFD, not to
// whatever extra character ICU's own (non-spec) Big5 table maps it to.
// https://github.com/nodejs/node/issues/40091
{
  const decoder = new TextDecoder('big5');
  const result = decoder.decode(Uint8Array.from([0x41, 0x42, 0x83, 0x5c, 0x43, 0x44]));
  assert.deepStrictEqual(codePoints(result), [0x41, 0x42, 0xfffd, 0x5c, 0x43, 0x44]);
}

// A lead byte with no trailing byte (end of input) is also an error.
{
  const decoder = new TextDecoder('big5');
  assert.deepStrictEqual(codePoints(decoder.decode(Uint8Array.from([0xa4]))), [0xfffd]);
}

// `fatal: true` must throw instead of substituting U+FFFD, and must
// actually recognize this sequence as invalid (unlike plain ICU, which
// treats it as valid and never throws even in fatal mode).
{
  const decoder = new TextDecoder('big5', { fatal: true });
  assert.throws(() => {
    decoder.decode(Uint8Array.from([0x83, 0x5c]));
  }, { name: 'TypeError', code: 'ERR_ENCODING_INVALID_ENCODED_DATA' });
}

// The four Big5 pointers that map to two combining code points instead of
// one, per the spec's special-cased steps ahead of the index table lookup.
{
  const decoder = new TextDecoder('big5');
  assert.deepStrictEqual(codePoints(decoder.decode(Uint8Array.from([0x88, 0x62]))), [0xca, 0x0304]);
  assert.deepStrictEqual(codePoints(decoder.decode(Uint8Array.from([0x88, 0x64]))), [0xca, 0x030c]);
  assert.deepStrictEqual(codePoints(decoder.decode(Uint8Array.from([0x88, 0xa3]))), [0xea, 0x0304]);
  assert.deepStrictEqual(codePoints(decoder.decode(Uint8Array.from([0x88, 0xa5]))), [0xea, 0x030c]);
}

// Streaming: a valid 2-byte sequence split across chunk boundaries must
// still decode correctly, and must not be flushed early.
{
  const decoder = new TextDecoder('big5');
  const r1 = decoder.decode(Uint8Array.from([0xa4]), { stream: true });
  assert.strictEqual(r1, '');
  const r2 = decoder.decode(Uint8Array.from([0x40]));
  assert.strictEqual(r2, '一');
}

// A supplementary-plane code point (outside the BMP) must be encoded as a
// surrogate pair in the resulting JS string.
{
  // Find a Big5 pointer whose code point is astral, by scanning the same
  // byte ranges the decoder itself accepts.
  const decoder = new TextDecoder('big5');
  let found = false;
  outer:
  for (let lead = 0x81; lead <= 0xfe && !found; lead++) {
    for (let byte = 0x40; byte <= 0xfe; byte++) {
      if (!((byte >= 0x40 && byte <= 0x7e) || (byte >= 0xa1 && byte <= 0xfe))) continue;
      const result = decoder.decode(Uint8Array.from([lead, byte]));
      if (result.length === 2 && codePoints(result).length === 1) {
        assert.ok(codePoints(result)[0] > 0xffff);
        found = true;
        break outer;
      }
    }
  }
  assert.ok(found, 'expected to find at least one astral Big5 mapping');
}
