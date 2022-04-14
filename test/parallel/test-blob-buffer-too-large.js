// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { Blob } = require('buffer');

if (common.isFreeBSD)
  common.skip('Oversized buffer make the FreeBSD CI runner crash');

try {
  new Blob([new Uint8Array(0xffffffff), [1]]);
} catch (e) {
  if (
    e.message === 'Array buffer allocation failed' ||
    e.message === 'Invalid typed array length: 4294967295'
  ) {
    common.skip(
      'Insufficient memory on this platform for oversized buffer test.'
    );
  } else {
    assert.strictEqual(e.code, 'ERR_BUFFER_TOO_LARGE');
  }
}
