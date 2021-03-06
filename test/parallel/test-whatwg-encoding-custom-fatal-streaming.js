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
    { encoding: 'utf-16be', sequence: [0x00] }
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
