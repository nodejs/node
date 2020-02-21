'use strict';

// Tests to verify signed integers are correctly written

require('../common');
const assert = require('assert');
const errorOutOfBounds = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: new RegExp('^The value of "value" is out of range\\. ' +
                      'It must be >= -\\d+ and <= \\d+\\. Received .+$')
};

// Test 8 bit
{
  const buffer = Buffer.alloc(2);

  buffer.writeInt8(0x23, 0);
  buffer.writeInt8(-5, 1);
  assert.ok(buffer.equals(new Uint8Array([ 0x23, 0xfb ])));

  /* Make sure we handle min/max correctly */
  buffer.writeInt8(0x7f, 0);
  buffer.writeInt8(-0x80, 1);
  assert.ok(buffer.equals(new Uint8Array([ 0x7f, 0x80 ])));

  assert.throws(() => {
    buffer.writeInt8(0x7f + 1, 0);
  }, errorOutOfBounds);
  assert.throws(() => {
    buffer.writeInt8(-0x80 - 1, 0);
  }, errorOutOfBounds);

  // Verify that default offset works fine.
  buffer.writeInt8(23, undefined);
  buffer.writeInt8(23);

  ['', '0', null, {}, [], () => {}, true, false].forEach((off) => {
    assert.throws(
      () => buffer.writeInt8(23, off),
      { code: 'ERR_INVALID_ARG_TYPE' });
  });

  [NaN, Infinity, -1, 1.01].forEach((off) => {
    assert.throws(
      () => buffer.writeInt8(23, off),
      { code: 'ERR_OUT_OF_RANGE' });
  });
}

// Test 16 bit
{
  const buffer = Buffer.alloc(4);

  buffer.writeInt16BE(0x0023, 0);
  buffer.writeInt16LE(0x0023, 2);
  assert.ok(buffer.equals(new Uint8Array([ 0x00, 0x23, 0x23, 0x00 ])));

  buffer.writeInt16BE(-5, 0);
  buffer.writeInt16LE(-5, 2);
  assert.ok(buffer.equals(new Uint8Array([ 0xff, 0xfb, 0xfb, 0xff ])));

  buffer.writeInt16BE(-1679, 0);
  buffer.writeInt16LE(-1679, 2);
  assert.ok(buffer.equals(new Uint8Array([ 0xf9, 0x71, 0x71, 0xf9 ])));

  /* Make sure we handle min/max correctly */
  buffer.writeInt16BE(0x7fff, 0);
  buffer.writeInt16BE(-0x8000, 2);
  assert.ok(buffer.equals(new Uint8Array([ 0x7f, 0xff, 0x80, 0x00 ])));

  buffer.writeInt16LE(0x7fff, 0);
  buffer.writeInt16LE(-0x8000, 2);
  assert.ok(buffer.equals(new Uint8Array([ 0xff, 0x7f, 0x00, 0x80 ])));

  ['writeInt16BE', 'writeInt16LE'].forEach((fn) => {

    // Verify that default offset works fine.
    buffer[fn](23, undefined);
    buffer[fn](23);

    assert.throws(() => {
      buffer[fn](0x7fff + 1, 0);
    }, errorOutOfBounds);
    assert.throws(() => {
      buffer[fn](-0x8000 - 1, 0);
    }, errorOutOfBounds);

    ['', '0', null, {}, [], () => {}, true, false].forEach((off) => {
      assert.throws(
        () => buffer[fn](23, off),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [NaN, Infinity, -1, 1.01].forEach((off) => {
      assert.throws(
        () => buffer[fn](23, off),
        { code: 'ERR_OUT_OF_RANGE' });
    });
  });
}

// Test 32 bit
{
  const buffer = Buffer.alloc(8);

  buffer.writeInt32BE(0x23, 0);
  buffer.writeInt32LE(0x23, 4);
  assert.ok(buffer.equals(new Uint8Array([
    0x00, 0x00, 0x00, 0x23, 0x23, 0x00, 0x00, 0x00
  ])));

  buffer.writeInt32BE(-5, 0);
  buffer.writeInt32LE(-5, 4);
  assert.ok(buffer.equals(new Uint8Array([
    0xff, 0xff, 0xff, 0xfb, 0xfb, 0xff, 0xff, 0xff
  ])));

  buffer.writeInt32BE(-805306713, 0);
  buffer.writeInt32LE(-805306713, 4);
  assert.ok(buffer.equals(new Uint8Array([
    0xcf, 0xff, 0xfe, 0xa7, 0xa7, 0xfe, 0xff, 0xcf
  ])));

  /* Make sure we handle min/max correctly */
  buffer.writeInt32BE(0x7fffffff, 0);
  buffer.writeInt32BE(-0x80000000, 4);
  assert.ok(buffer.equals(new Uint8Array([
    0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00
  ])));

  buffer.writeInt32LE(0x7fffffff, 0);
  buffer.writeInt32LE(-0x80000000, 4);
  assert.ok(buffer.equals(new Uint8Array([
    0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x80
  ])));

  ['writeInt32BE', 'writeInt32LE'].forEach((fn) => {

    // Verify that default offset works fine.
    buffer[fn](23, undefined);
    buffer[fn](23);

    assert.throws(() => {
      buffer[fn](0x7fffffff + 1, 0);
    }, errorOutOfBounds);
    assert.throws(() => {
      buffer[fn](-0x80000000 - 1, 0);
    }, errorOutOfBounds);

    ['', '0', null, {}, [], () => {}, true, false].forEach((off) => {
      assert.throws(
        () => buffer[fn](23, off),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [NaN, Infinity, -1, 1.01].forEach((off) => {
      assert.throws(
        () => buffer[fn](23, off),
        { code: 'ERR_OUT_OF_RANGE' });
    });
  });
}

// Test 48 bit
{
  const value = 0x1234567890ab;
  const buffer = Buffer.allocUnsafe(6);
  buffer.writeIntBE(value, 0, 6);
  assert.ok(buffer.equals(new Uint8Array([
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab
  ])));

  buffer.writeIntLE(value, 0, 6);
  assert.ok(buffer.equals(new Uint8Array([
    0xab, 0x90, 0x78, 0x56, 0x34, 0x12
  ])));
}

// Test Int
{
  const data = Buffer.alloc(8);

  // Check byteLength.
  ['writeIntBE', 'writeIntLE'].forEach((fn) => {
    ['', '0', null, {}, [], () => {}, true, false, undefined].forEach((bl) => {
      assert.throws(
        () => data[fn](23, 0, bl),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [Infinity, -1].forEach((byteLength) => {
      assert.throws(
        () => data[fn](23, 0, byteLength),
        {
          code: 'ERR_OUT_OF_RANGE',
          message: 'The value of "byteLength" is out of range. ' +
                   `It must be >= 1 and <= 6. Received ${byteLength}`
        }
      );
    });

    [NaN, 1.01].forEach((byteLength) => {
      assert.throws(
        () => data[fn](42, 0, byteLength),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError',
          message: 'The value of "byteLength" is out of range. ' +
                   `It must be an integer. Received ${byteLength}`
        });
    });
  });

  // Test 1 to 6 bytes.
  for (let i = 1; i <= 6; i++) {
    ['writeIntBE', 'writeIntLE'].forEach((fn) => {
      const min = -(2 ** (i * 8 - 1));
      const max = 2 ** (i * 8 - 1) - 1;
      let range = `>= ${min} and <= ${max}`;
      if (i > 4) {
        range = `>= -(2 ** ${i * 8 - 1}) and < 2 ** ${i * 8 - 1}`;
      }
      [min - 1, max + 1].forEach((val) => {
        const received = i > 4 ?
          String(val).replace(/(\d)(?=(\d\d\d)+(?!\d))/g, '$1_') :
          val;
        assert.throws(() => {
          data[fn](val, 0, i);
        }, {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError',
          message: 'The value of "value" is out of range. ' +
                   `It must be ${range}. Received ${received}`
        });
      });

      ['', '0', null, {}, [], () => {}, true, false, undefined].forEach((o) => {
        assert.throws(
          () => data[fn](min, o, i),
          {
            code: 'ERR_INVALID_ARG_TYPE',
            name: 'TypeError'
          });
      });

      [Infinity, -1, -4294967295].forEach((offset) => {
        assert.throws(
          () => data[fn](min, offset, i),
          {
            code: 'ERR_OUT_OF_RANGE',
            name: 'RangeError',
            message: 'The value of "offset" is out of range. ' +
                     `It must be >= 0 and <= ${8 - i}. Received ${offset}`
          });
      });

      [NaN, 1.01].forEach((offset) => {
        assert.throws(
          () => data[fn](max, offset, i),
          {
            code: 'ERR_OUT_OF_RANGE',
            name: 'RangeError',
            message: 'The value of "offset" is out of range. ' +
                     `It must be an integer. Received ${offset}`
          });
      });
    });
  }
}
