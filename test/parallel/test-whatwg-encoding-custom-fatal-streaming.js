'use strict';

// From: https://github.com/w3c/web-platform-tests/blob/d74324b53c/encoding/textdecoder-fatal-streaming.html
// With the twist that we specifically test for Node.js error codes

const common = require('../common');
const assert = require('assert');

if (!common.hasIntl)
  common.skip('missing Intl');

{
  [
    { encoding: 'utf-8', sequence: [0xC0] },
    { encoding: 'utf-16le', sequence: [0x00] },
    { encoding: 'utf-16be', sequence: [0x00] },
  ].forEach((testCase) => {
    const data = new Uint8Array([testCase.sequence]);
    assert.throws(
      () => {
        const decoder = new TextDecoder(testCase.encoding, { fatal: true });
        decoder.decode(data);
      }, {
        code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
        name: 'TypeError',
        message:
          `The encoded data was not valid for encoding ${testCase.encoding}`
      }
    );
  });
}

{
  const decoder = new TextDecoder('utf-16le', { fatal: true });
  const odd = new Uint8Array([0x00]);
  const even = new Uint8Array([0x00, 0x00]);

  assert.throws(
    () => {
      decoder.decode(even, { stream: true });
      decoder.decode(odd);
    }, {
      code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
      name: 'TypeError',
      message:
        'The encoded data was not valid for encoding utf-16le'
    }
  );

  assert.throws(
    () => {
      decoder.decode(odd, { stream: true });
      decoder.decode(even);
    }, {
      code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
      name: 'TypeError',
      message:
        'The encoded data was not valid for encoding utf-16le'
    }
  );
}

// Errors reset the decoder, so the next call starts fresh.
// Refs: https://github.com/whatwg/encoding/pull/369
{
  const testCases = [
    // Pending bytes are dropped.
    { encoding: 'utf-8', first: [0xE2, 0x82], bad: [0x41],
      next: [0x41], expected: 'A' },
    { encoding: 'utf-16le', first: [0x3D, 0xD8], bad: [0x41, 0x00],
      next: [0x41, 0x00], expected: 'A' },
    { encoding: 'utf-16be', first: [0xD8, 0x3D], bad: [0x00, 0x41],
      next: [0x00, 0x41], expected: 'A' },
    // The BOM is stripped again.
    { encoding: 'utf-8', first: [0xEF, 0xBB, 0xBF, 0x41], bad: [0xFF],
      next: [0xEF, 0xBB, 0xBF, 0x42], expected: 'B' },
    { encoding: 'utf-16le', first: [0xFF, 0xFE, 0x41, 0x00],
      bad: [0x00, 0xD8, 0x41, 0x00], next: [0xFF, 0xFE, 0x42, 0x00],
      expected: 'B' },
  ];

  // Non-Unicode encodings need full ICU.
  if (common.hasFullICU) {
    testCases.push(
      { encoding: 'gb18030', first: [0x81, 0x30], bad: [0x41],
        next: [0x41], expected: 'A' },
      // ESC $ B switches to JIS X 0208.
      { encoding: 'iso-2022-jp', first: [0x1B, 0x24, 0x42, 0x30, 0x21],
        bad: [0x0E], next: [0x41, 0x42], expected: 'AB' },
    );
  }

  testCases.forEach(({ encoding, first, bad, next, expected }) => {
    const decoder = new TextDecoder(encoding, { fatal: true });
    decoder.decode(new Uint8Array(first), { stream: true });
    assert.throws(
      () => decoder.decode(new Uint8Array(bad), { stream: true }),
      { code: 'ERR_ENCODING_INVALID_ENCODED_DATA', name: 'TypeError' }
    );
    assert.strictEqual(decoder.decode(new Uint8Array(next)), expected);
  });
}
