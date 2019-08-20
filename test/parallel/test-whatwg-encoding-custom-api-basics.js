'use strict';

// From: https://github.com/w3c/web-platform-tests/blob/master/encoding/api-basics.html
// This is the part that can be run without ICU

require('../common');

const assert = require('assert');

function testDecodeSample(encoding, string, bytes) {
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes)),
    string);
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes).buffer),
    string);
}

// `z` (ASCII U+007A), cent (Latin-1 U+00A2), CJK water (BMP U+6C34),
// G-Clef (non-BMP U+1D11E), PUA (BMP U+F8FF), PUA (non-BMP U+10FFFD)
// byte-swapped BOM (non-character U+FFFE)
const sample = 'z\xA2\u6C34\uD834\uDD1E\uF8FF\uDBFF\uDFFD\uFFFE';

{
  const encoding = 'utf-8';
  const string = sample;
  const bytes = [
    0x7A, 0xC2, 0xA2, 0xE6, 0xB0, 0xB4,
    0xF0, 0x9D, 0x84, 0x9E, 0xEF, 0xA3,
    0xBF, 0xF4, 0x8F, 0xBF, 0xBD, 0xEF,
    0xBF, 0xBE
  ];
  const encoded = new TextEncoder().encode(string);
  assert.deepStrictEqual([].slice.call(encoded), bytes);
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes)),
    string);
  assert.strictEqual(
    new TextDecoder(encoding).decode(new Uint8Array(bytes).buffer),
    string);
}

testDecodeSample(
  'utf-16le',
  sample,
  [
    0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C,
    0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xF8,
    0xFF, 0xDB, 0xFD, 0xDF, 0xFE, 0xFF
  ]
);

testDecodeSample(
  'utf-16',
  sample,
  [
    0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C,
    0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xF8,
    0xFF, 0xDB, 0xFD, 0xDF, 0xFE, 0xFF
  ]
);
