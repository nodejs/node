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

{
  const ab32 = new ArrayBuffer(32);
  const dv32 = new DataView(ab32);
  dv32.setUint32(0, 1);
  dv32.setUint32(4, 1, true);
  dv32.setBigUint64(8, 1n);
  dv32.setBigUint64(16, 1n, true);
  dv32.setUint16(24, 1);
  dv32.setUint16(26, 1, true);
  dv32.setUint8(28, 1);
  dv32.setUint8(29, 1);

  // 'should consider equal buffers to be equal'
  assert.strictEqual(
    crypto.timingSafeEqual(Buffer.from(ab32), dv32),
    true
  );
  assert.strictEqual(
    crypto.timingSafeEqual(new Uint32Array(ab32), dv32),
    true
  );
  assert.strictEqual(
    crypto.timingSafeEqal(
      new Uint8Array(ab32, 0, 16),
      Buffer.from(ab32, 0, 16)
    ),
    true
  );
  assert.strictEqual(
    crypto.timingSafeEqual(
      new Uint32Array(ab32, 0, 8),
      Buffer.of(0, 0, 0, 1, // 4
                1, 0, 0, 0, // 8
                0, 0, 0, 0, 0, 0, 0, 1, // 16
                1, 0, 0, 0, 0, 0, 0, 0, // 24
                0, 1, // 26
                1, 0, // 28
                1, 1, // 30
                0, 0) // 32
    ),
    true
  );

  // 'should consider unequal buffer views to be unequal'
  assert.strictEqual(
    crypto.timingSafeEqual(
      new Uint32Array(ab32, 16, 4),
      Buffer.from(ab32, 0, 16)
    ),
    false
  );
  assert.strictEqual(
    crypto.timingSafeEqual(
      new Uint8Array(ab32, 0, 16),
      Buffer.from(ab32, 16, 16)
    ),
    false
  );
  assert.strictEqual(
    crypto.timingSafeEqual(
      new Uint32Array(ab32, 0, 8),
      Buffer.of(0, 0, 0, 1, // 4
                1, 0, 0, 0, // 8
                0, 0, 0, 0, 0, 0, 0, 1, // 16
                1, 0, 0, 0, 0, 0, 0, 0, // 24
                0, 1, // 26
                1, 0, // 28
                1, 1, // 30
                0, 1) // 32
    ),
    false
  );
  // 'buffers with differing byteLength must throw an equal length error'
  common.expectsError(
    () => crypto.timingSafeEqual(Buffer.from(ab32, 0, 8),
                                 Buffer.from(ab32, 0, 9)),
    {
      code: 'ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH',
      type: RangeError,
      message: 'Input buffers must have the same byteLength'
    }
  );
  common.expectsError(
    () => crypto.timingSafeEqual(
      new Uint32Array(ab32, 0, 8), // 32
      Buffer.of(0, 0, 0, 1, // 4
                1, 0, 0, 0, // 8
                0, 0, 0, 0, 0, 0, 0, 1, // 16
                1, 0, 0, 0, 0, 0, 0, 0, // 24
                0, 1, // 26
                1, 0, // 28
                1, 1, // 30
                0) // 31
    ),
    {
      code: 'ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH',
      type: RangeError,
      message: 'Input buffers must have the same byteLength'
    }
  );
}

common.expectsError(
  () => crypto.timingSafeEqual(Buffer.from([1, 2, 3]), Buffer.from([1, 2])),
  {
    code: 'ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH',
    type: RangeError,
    message: 'Input buffers must have the same byteLength'
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
