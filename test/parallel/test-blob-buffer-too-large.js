// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { Blob } = require('buffer');

try {
  new Blob([new Uint8Array(0xffffffff), [1]]);
} catch (e) {
  if (e.message === 'Array buffer allocation failed') {
    common.skip(
      'Insufficient memory on this platform for oversized buffer test.'
    );
  } else {
    assert.strictEqual(e.code, 'ERR_BUFFER_TOO_LARGE');
  }
}
