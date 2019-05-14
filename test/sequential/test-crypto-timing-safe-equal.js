'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// 'should consider equal strings to be equal'
assert.strictEqual(
  crypto.timingSafeEqual(Buffer.from('foo'), Buffer.from('foo')),
  true
);

// 'should consider unequal strings to be unequal'
assert.strictEqual(
  crypto.timingSafeEqual(Buffer.from('foo'), Buffer.from('bar')),
  false
);

common.expectsError(
  () => crypto.timingSafeEqual(Buffer.from([1, 2, 3]), Buffer.from([1, 2])),
  {
    code: 'ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH',
    type: RangeError,
    message: 'Input buffers must have the same length'
  }
);

common.expectsError(
  () => crypto.timingSafeEqual('not a buffer', Buffer.from([1, 2])),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message:
      'The "buf1" argument must be one of type Buffer, TypedArray, or ' +
      'DataView. Received type string'
  }
);

common.expectsError(
  () => crypto.timingSafeEqual(Buffer.from([1, 2]), 'not a buffer'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message:
      'The "buf2" argument must be one of type Buffer, TypedArray, or ' +
      'DataView. Received type string'
  }
);
