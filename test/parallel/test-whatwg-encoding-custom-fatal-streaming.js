'use strict';

// From: https://github.com/w3c/web-platform-tests/blob/d74324b53c/encoding/textdecoder-fatal-streaming.html
// With the twist that we specifically test for Node.js error codes

const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const assert = require('assert');

{
  [
    { encoding: 'utf-8', sequence: [0xC0] },
    { encoding: 'utf-16le', sequence: [0x00] },
    { encoding: 'utf-16be', sequence: [0x00] }
  ].forEach((testCase) => {
    const data = new Uint8Array([testCase.sequence]);
    common.expectsError(
      () => {
        const decoder = new TextDecoder(testCase.encoding, { fatal: true });
        decoder.decode(data);
      }, {
        code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
        type: TypeError,
        message:
          `The encoded data was not valid for encoding ${testCase.encoding}`
      }
    );

    // TODO(joyeecheung): remove this when WPT is ported
    assert.strictEqual(
      new TextDecoder(testCase.encoding).decode(data),
      '\uFFFD'
    );
  });
}

{
  const decoder = new TextDecoder('utf-16le', { fatal: true });
  const odd = new Uint8Array([0x00]);
  const even = new Uint8Array([0x00, 0x00]);

  // TODO(joyeecheung): remove this when WPT is ported
  assert.strictEqual(decoder.decode(odd, { stream: true }), '');
  assert.strictEqual(decoder.decode(odd), '\u0000');

  common.expectsError(
    () => {
      decoder.decode(even, { stream: true });
      decoder.decode(odd);
    }, {
      code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
      type: TypeError,
      message:
        'The encoded data was not valid for encoding utf-16le'
    }
  );

  common.expectsError(
    () => {
      decoder.decode(odd, { stream: true });
      decoder.decode(even);
    }, {
      code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
      type: TypeError,
      message:
        'The encoded data was not valid for encoding utf-16le'
    }
  );

  // TODO(joyeecheung): remove this when WPT is ported
  assert.strictEqual(decoder.decode(even, { stream: true }), '\u0000');
  assert.strictEqual(decoder.decode(even), '\u0000');
}
