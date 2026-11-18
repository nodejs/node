'use strict';

// Only the first scalar value a streaming decoder outputs is checked for a
// BOM. A U+FEFF that follows a sequence completed from bytes held over from
// a previous call must be kept.
// Refs: https://github.com/nodejs/node/issues/67561

const common = require('../common');

const assert = require('assert');

const BOM = '﻿';

// [held-over bytes, next chunk, expected output of the next chunk]
const cases = [
  [[0xC3], [0xA9, 0xEF, 0xBB, 0xBF], `é${BOM}`],
  [[0xE2, 0x82], [0xAC, 0xEF, 0xBB, 0xBF], `€${BOM}`],
  [[0xE2], [0x82, 0xAC, 0xEF, 0xBB, 0xBF], `€${BOM}`],
  [[0xF0, 0x9F], [0x98, 0x80, 0xEF, 0xBB, 0xBF], `\u{1F600}${BOM}`],
];

for (const [first, second, expected] of cases) {
  for (const ignoreBOM of [false, true]) {
    const decoder = new TextDecoder('utf-8', { ignoreBOM });
    assert.strictEqual(
      decoder.decode(new Uint8Array(first), { stream: true }), '');
    assert.strictEqual(
      decoder.decode(new Uint8Array(second), { stream: true }), expected);
    assert.strictEqual(decoder.decode(), '');
  }
}

// A real BOM split across calls is stripped, unless ignoreBOM is set.
for (const [ignoreBOM, expected] of [[false, 'ab'], [true, `${BOM}ab`]]) {
  const decoder = new TextDecoder('utf-8', { ignoreBOM });
  assert.strictEqual(decoder.decode(new Uint8Array([0xEF]), { stream: true }), '');
  assert.strictEqual(
    decoder.decode(new Uint8Array([0xBB, 0xBF, 0x61, 0x62]), { stream: true }),
    expected);
  assert.strictEqual(decoder.decode(), '');
}

// TextDecoderStream goes through the same path.
(async () => {
  const stream = new TextDecoderStream();
  const writer = stream.writable.getWriter();
  writer.write(new Uint8Array([0xE2, 0x82]));
  writer.write(new Uint8Array([0xAC, 0xEF, 0xBB, 0xBF]));
  writer.close();
  let out = '';
  for await (const chunk of stream.readable) out += chunk;
  assert.strictEqual(out, `€${BOM}`);
})().then(common.mustCall());
