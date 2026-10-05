'use strict';

require('../common');
const assert = require('assert');

// `encodeInto()` must write as many code points as fit in the destination.
// Code points in U+0080..U+07FF occupy two UTF-8 bytes, so a destination of
// N bytes must take floor(N / 2) of them. The internal helper that decides
// where to stop near the buffer boundary treated the two byte range as
// U+0080..U+03FF, so anything from U+0400 upwards was costed as three bytes
// and the destination was left short.

const encoder = new TextEncoder();

// One representative per affected block: Cyrillic, Hebrew, and the last
// code point that still encodes to two bytes.
for (const char of ['Ѐ', 'б', 'א', '߿']) {
  assert.strictEqual(Buffer.byteLength(char, 'utf8'), 2);

  const input = char.repeat(64);
  for (const destLength of [2, 3, 4, 10, 40, 100]) {
    const dest = new Uint8Array(destLength);
    const { read, written } = encoder.encodeInto(input, dest);

    const expectedRead = Math.floor(destLength / 2);
    const message =
      `U+${char.codePointAt(0).toString(16).toUpperCase().padStart(4, '0')} ` +
      `into ${destLength} bytes`;

    assert.strictEqual(read, expectedRead, `read for ${message}`);
    assert.strictEqual(written, expectedRead * 2, `written for ${message}`);

    // The bytes that were written must be the correct encoding.
    assert.deepStrictEqual(
      dest.subarray(0, written),
      new Uint8Array(Buffer.from(char.repeat(expectedRead), 'utf8')),
      `bytes for ${message}`,
    );
  }
}

// A three byte code point still costs three bytes.
{
  const dest = new Uint8Array(4);
  const { read, written } = encoder.encodeInto('ࠀࠀ', dest);
  assert.strictEqual(read, 1);
  assert.strictEqual(written, 3);
}
