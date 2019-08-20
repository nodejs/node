'use strict';
// From: https://github.com/w3c/web-platform-tests/blob/master/encoding/api-invalid-label.html
// With the twist that we specifically test for Node.js error codes

const common = require('../common');

[
  'utf-8',
  'unicode-1-1-utf-8',
  'utf8',
  'utf-16be',
  'utf-16le',
  'utf-16'
].forEach((i) => {
  ['\u0000', '\u000b', '\u00a0', '\u2028', '\u2029'].forEach((ws) => {
    common.expectsError(
      () => new TextDecoder(`${ws}${i}`),
      {
        code: 'ERR_ENCODING_NOT_SUPPORTED',
        type: RangeError
      }
    );

    common.expectsError(
      () => new TextDecoder(`${i}${ws}`),
      {
        code: 'ERR_ENCODING_NOT_SUPPORTED',
        type: RangeError
      }
    );

    common.expectsError(
      () => new TextDecoder(`${ws}${i}${ws}`),
      {
        code: 'ERR_ENCODING_NOT_SUPPORTED',
        type: RangeError
      }
    );
  });
});
