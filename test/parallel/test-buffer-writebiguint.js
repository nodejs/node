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

// Test 64 bit
{
  const buffer = Buffer.alloc(16);

  buffer.writeBigUInt64BE(0x0123456789ABCDEFn, 0);
  buffer.writeBigUInt64LE(0x0123456789ABCDEFn, 8);
  assert.ok(buffer.equals(Uint8Array.of(
    1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 1
  )));

  // Handling of min/max
  buffer.writeBigUInt64BE(0x7FFFFFFFFFFFFFFFn, 0);
  buffer.writeBigUInt64LE(0n, 8);

  assert.ok(buffer.equals(Uint8Array.of(
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  )));

  ['writeBigUInt64BE', 'writeBigUInt64LE'].forEach((fn) => {

    // Verify that default offset works fine.
    buffer[fn](23n, undefined);
    buffer[fn](23n);

    assert.throws(() => {
      buffer[fn](0x7FFFFFFFFFFFFFFFn + 1n, 0);
    }, errorOutOfBounds);
    assert.throws(() => {
      buffer[fn](-0x8000000000000000n - 1n, 0);
    }, errorOutOfBounds);

    ['', '0', null, {}, [], () => {}, true, false, 0n].forEach((off) => {
      assert.throws(
        () => buffer[fn](23n, off),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [NaN, Infinity, -1, 1.01].forEach((off) => {
      assert.throws(
        () => buffer[fn](23n, off),
        { code: 'ERR_OUT_OF_RANGE' });
    });
  });
}

// Test 128 bit
{
  const buffer = Buffer.alloc(32);

  buffer.writeBigUInt128BE(0x0123456789ABCDEF0123456789ABCDEFn, 0);
  buffer.writeBigUInt128LE(0x0123456789ABCDEF0123456789ABCDEFn, 16);
  assert.ok(buffer.equals(Uint8Array.of(
    1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    1, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 1,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 1
  )));

  // Handling of min/max
  buffer.writeBigUInt64BE(0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFn, 0);
  buffer.writeBigUInt64LE(0n, 16);

  assert.ok(buffer.equals(Uint8Array.of(
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  )));

  ['writeBigUInt128BE', 'writeBigUInt128LE'].forEach((fn) => {

    // Verify that default offset works fine.
    buffer[fn](23n, undefined);
    buffer[fn](23n);

    assert.throws(() => {
      buffer[fn](0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFn + 1n, 0);
    }, errorOutOfBounds);
    assert.throws(() => {
      buffer[fn](0n - 1n, 0);
    }, errorOutOfBounds);

    ['', '0', null, {}, [], () => {}, true, false, 0n].forEach((off) => {
      assert.throws(
        () => buffer[fn](23n, off),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [NaN, Infinity, -1, 1.01].forEach((off) => {
      assert.throws(
        () => buffer[fn](23n, off),
        { code: 'ERR_OUT_OF_RANGE' });
    });
  });
}
