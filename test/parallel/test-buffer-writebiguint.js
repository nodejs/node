'use strict';

// Tests to verify signed integers are correctly written

const common = require('../common');
const assert = require('assert');
const errorOutOfBounds = common.expectsError({
  code: 'ERR_OUT_OF_RANGE',
  type: RangeError,
  message: new RegExp('^The value of "value" is out of range\\. ' +
                      'It must be >= -\\d+ and <= \\d+\\. Received .+$')
}, 10);

// Test 128 bit
{
  const buffer = Buffer.alloc(16);

  buffer.writeBigUInt64BE(0x0123456789ABCDEFn, 0);
  buffer.writeBigUInt64LE(0x0123456789ABCDEFn, 8);
  assert.ok(buffer.equals(Uint8Array.of(
    1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
  )))

}
