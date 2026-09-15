'use strict';

require('../common');
const assert = require('assert');

const encoder = new TextEncoder();

function assertExactFit(character, expectedBytes) {
  const input = character.repeat(33);
  const destination = new Uint8Array(expectedBytes.length);

  assert.deepStrictEqual(
    encoder.encodeInto(input, destination),
    { read: character.length, written: expectedBytes.length });
  assert.deepStrictEqual(destination, Uint8Array.from(expectedBytes));

  assert.deepStrictEqual(
    encoder.encodeInto(input, new Uint8Array(expectedBytes.length - 1)),
    { read: 0, written: 0 });
}

// Exercise the optimized path with UTF-8 length boundaries.
assertExactFit('\u03ff', [0xcf, 0xbf]);
assertExactFit('\u0400', [0xd0, 0x80]);
assertExactFit('\u07ff', [0xdf, 0xbf]);
assertExactFit('\u0800', [0xe0, 0xa0, 0x80]);

// One-byte V8 strings must treat Latin-1 code units as unsigned.
assertExactFit('\x80', [0xc2, 0x80]);
assertExactFit('\xff', [0xc3, 0xbf]);

// Appending a two-byte character changes V8's string representation but must
// not affect how much of the preceding text can be encoded.
{
  const destination = new Uint8Array(2);
  assert.deepStrictEqual(
    encoder.encodeInto(`${'\xe9'.repeat(33)}\u263a`, destination),
    { read: 1, written: 2 });
  assert.deepStrictEqual(destination, Uint8Array.from([0xc3, 0xa9]));
}

// Keep surrogate handling covered when validation happens during conversion.
assertExactFit('\ud83d\ude00', [0xf0, 0x9f, 0x98, 0x80]);
assertExactFit('\ud800', [0xef, 0xbf, 0xbd]);

// Continue filling the destination after a surrogate pair in the scalar tail.
{
  const input = '\u0800\u0800\ud83d\ude00a'.repeat(7);
  const destination = new Uint8Array(22);
  assert.deepStrictEqual(
    encoder.encodeInto(input, destination),
    { read: 10, written: 22 });
  assert.deepStrictEqual(destination, encoder.encode(input.slice(0, 10)));
}
