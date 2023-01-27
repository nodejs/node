'use strict';

require('../common');
const assert = require('assert');
const { isUtf8, Buffer } = require('buffer');
const { TextEncoder } = require('util');

const encoder = new TextEncoder();

assert.strictEqual(isUtf8(encoder.encode('hello')), true);
assert.strictEqual(isUtf8(encoder.encode('ğ')), true);
assert.strictEqual(isUtf8(Buffer.from([])), true);

// Taken from test/fixtures/wpt/encoding/textdecoder-fatal.any.js
[
  [0xFF], // 'invalid code'
  [0xC0], // 'ends early'
  [0xE0], // 'ends early 2'
  [0xC0, 0x00], // 'invalid trail'
  [0xC0, 0xC0], // 'invalid trail 2'
  [0xE0, 0x00], // 'invalid trail 3'
  [0xE0, 0xC0], // 'invalid trail 4'
  [0xE0, 0x80, 0x00], // 'invalid trail 5'
  [0xE0, 0x80, 0xC0], // 'invalid trail 6'
  [0xFC, 0x80, 0x80, 0x80, 0x80, 0x80], // '> 0x10FFFF'
  [0xFE, 0x80, 0x80, 0x80, 0x80, 0x80], // 'obsolete lead byte'

  // Overlong encodings
  [0xC0, 0x80], // 'overlong U+0000 - 2 bytes'
  [0xE0, 0x80, 0x80], // 'overlong U+0000 - 3 bytes'
  [0xF0, 0x80, 0x80, 0x80], // 'overlong U+0000 - 4 bytes'
  [0xF8, 0x80, 0x80, 0x80, 0x80], // 'overlong U+0000 - 5 bytes'
  [0xFC, 0x80, 0x80, 0x80, 0x80, 0x80], // 'overlong U+0000 - 6 bytes'

  [0xC1, 0xBF], // 'overlong U+007F - 2 bytes'
  [0xE0, 0x81, 0xBF], // 'overlong U+007F - 3 bytes'
  [0xF0, 0x80, 0x81, 0xBF], // 'overlong U+007F - 4 bytes'
  [0xF8, 0x80, 0x80, 0x81, 0xBF], // 'overlong U+007F - 5 bytes'
  [0xFC, 0x80, 0x80, 0x80, 0x81, 0xBF], // 'overlong U+007F - 6 bytes'

  [0xE0, 0x9F, 0xBF], // 'overlong U+07FF - 3 bytes'
  [0xF0, 0x80, 0x9F, 0xBF], // 'overlong U+07FF - 4 bytes'
  [0xF8, 0x80, 0x80, 0x9F, 0xBF], // 'overlong U+07FF - 5 bytes'
  [0xFC, 0x80, 0x80, 0x80, 0x9F, 0xBF], // 'overlong U+07FF - 6 bytes'

  [0xF0, 0x8F, 0xBF, 0xBF], // 'overlong U+FFFF - 4 bytes'
  [0xF8, 0x80, 0x8F, 0xBF, 0xBF], // 'overlong U+FFFF - 5 bytes'
  [0xFC, 0x80, 0x80, 0x8F, 0xBF, 0xBF], // 'overlong U+FFFF - 6 bytes'

  [0xF8, 0x84, 0x8F, 0xBF, 0xBF], // 'overlong U+10FFFF - 5 bytes'
  [0xFC, 0x80, 0x84, 0x8F, 0xBF, 0xBF], // 'overlong U+10FFFF - 6 bytes'

  // UTF-16 surrogates encoded as code points in UTF-8
  [0xED, 0xA0, 0x80], // 'lead surrogate'
  [0xED, 0xB0, 0x80], // 'trail surrogate'
  [0xED, 0xA0, 0x80, 0xED, 0xB0, 0x80], // 'surrogate pair'
].forEach((input) => {
  assert.strictEqual(isUtf8(Buffer.from(input)), false);
});

[
  null,
  undefined,
  'hello',
  true,
  false,
].forEach((input) => {
  assert.throws(
    () => { isUtf8(input); },
    {
      code: 'ERR_INVALID_ARG_TYPE',
    },
  );
});

{
  // Test with detached array buffers
  const arrayBuffer = new ArrayBuffer(1024);
  structuredClone(arrayBuffer, { transfer: [arrayBuffer] });
  assert.throws(
    () => { isUtf8(arrayBuffer); },
    {
      code: 'ERR_INVALID_STATE'
    }
  );
}
