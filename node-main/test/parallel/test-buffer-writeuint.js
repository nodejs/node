'use strict';

require('../common');
const assert = require('assert');

// We need to check the following things:
//  - We are correctly resolving big endian (doesn't mean anything for 8 bit)
//  - Correctly resolving little endian (doesn't mean anything for 8 bit)
//  - Correctly using the offsets
//  - Correctly interpreting values that are beyond the signed range as unsigned

{ // OOB
  const data = Buffer.alloc(8);
  ['UInt8', 'UInt16BE', 'UInt16LE', 'UInt32BE', 'UInt32LE'].forEach((fn) => {

    // Verify that default offset works fine.
    data[`write${fn}`](23, undefined);
    data[`write${fn}`](23);

    ['', '0', null, {}, [], () => {}, true, false].forEach((o) => {
      assert.throws(
        () => data[`write${fn}`](23, o),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [NaN, Infinity, -1, 1.01].forEach((o) => {
      assert.throws(
        () => data[`write${fn}`](23, o),
        { code: 'ERR_OUT_OF_RANGE' });
    });
  });
}

{ // Test 8 bit
  const data = Buffer.alloc(4);

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  assert.ok(data.equals(new Uint8Array([23, 23, 23, 23])));

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  assert.ok(data.equals(new Uint8Array([23, 23, 23, 23])));

  data.writeUInt8(255, 0);
  assert.strictEqual(data[0], 255);

  data.writeUInt8(255, 0);
  assert.strictEqual(data[0], 255);
}

// Test 16 bit
{
  let value = 0x2343;
  const data = Buffer.alloc(4);

  data.writeUInt16BE(value, 0);
  assert.ok(data.equals(new Uint8Array([0x23, 0x43, 0, 0])));

  data.writeUInt16BE(value, 1);
  assert.ok(data.equals(new Uint8Array([0x23, 0x23, 0x43, 0])));

  data.writeUInt16BE(value, 2);
  assert.ok(data.equals(new Uint8Array([0x23, 0x23, 0x23, 0x43])));

  data.writeUInt16LE(value, 0);
  assert.ok(data.equals(new Uint8Array([0x43, 0x23, 0x23, 0x43])));

  data.writeUInt16LE(value, 1);
  assert.ok(data.equals(new Uint8Array([0x43, 0x43, 0x23, 0x43])));

  data.writeUInt16LE(value, 2);
  assert.ok(data.equals(new Uint8Array([0x43, 0x43, 0x43, 0x23])));

  value = 0xff80;
  data.writeUInt16LE(value, 0);
  assert.ok(data.equals(new Uint8Array([0x80, 0xff, 0x43, 0x23])));

  data.writeUInt16BE(value, 0);
  assert.ok(data.equals(new Uint8Array([0xff, 0x80, 0x43, 0x23])));

  value = 0xfffff;
  ['writeUInt16BE', 'writeUInt16LE'].forEach((fn) => {
    assert.throws(
      () => data[fn](value, 0),
      {
        code: 'ERR_OUT_OF_RANGE',
        message: 'The value of "value" is out of range. ' +
                 `It must be >= 0 and <= 65535. Received ${value}`
      }
    );
  });
}

// Test 32 bit
{
  const data = Buffer.alloc(6);
  const value = 0xe7f90a6d;

  data.writeUInt32BE(value, 0);
  assert.ok(data.equals(new Uint8Array([0xe7, 0xf9, 0x0a, 0x6d, 0, 0])));

  data.writeUInt32BE(value, 1);
  assert.ok(data.equals(new Uint8Array([0xe7, 0xe7, 0xf9, 0x0a, 0x6d, 0])));

  data.writeUInt32BE(value, 2);
  assert.ok(data.equals(new Uint8Array([0xe7, 0xe7, 0xe7, 0xf9, 0x0a, 0x6d])));

  data.writeUInt32LE(value, 0);
  assert.ok(data.equals(new Uint8Array([0x6d, 0x0a, 0xf9, 0xe7, 0x0a, 0x6d])));

  data.writeUInt32LE(value, 1);
  assert.ok(data.equals(new Uint8Array([0x6d, 0x6d, 0x0a, 0xf9, 0xe7, 0x6d])));

  data.writeUInt32LE(value, 2);
  assert.ok(data.equals(new Uint8Array([0x6d, 0x6d, 0x6d, 0x0a, 0xf9, 0xe7])));
}

// Test 48 bit
{
  const value = 0x1234567890ab;
  const data = Buffer.allocUnsafe(6);
  data.writeUIntBE(value, 0, 6);
  assert.ok(data.equals(new Uint8Array([0x12, 0x34, 0x56, 0x78, 0x90, 0xab])));

  data.writeUIntLE(value, 0, 6);
  assert.ok(data.equals(new Uint8Array([0xab, 0x90, 0x78, 0x56, 0x34, 0x12])));
}

// Test UInt
{
  const data = Buffer.alloc(8);
  let val = 0x100;

  // Check byteLength.
  ['writeUIntBE', 'writeUIntLE'].forEach((fn) => {
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
    const range = i < 5 ? `= ${val - 1}` : ` 2 ** ${i * 8}`;
    const received = i > 4 ?
      String(val).replace(/(\d)(?=(\d\d\d)+(?!\d))/g, '$1_') :
      val;
    ['writeUIntBE', 'writeUIntLE'].forEach((fn) => {
      assert.throws(() => {
        data[fn](val, 0, i);
      }, {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: 'The value of "value" is out of range. ' +
                 `It must be >= 0 and <${range}. Received ${received}`
      });

      ['', '0', null, {}, [], () => {}, true, false].forEach((o) => {
        assert.throws(
          () => data[fn](23, o, i),
          {
            code: 'ERR_INVALID_ARG_TYPE',
            name: 'TypeError'
          });
      });

      [Infinity, -1, -4294967295].forEach((offset) => {
        assert.throws(
          () => data[fn](val - 1, offset, i),
          {
            code: 'ERR_OUT_OF_RANGE',
            name: 'RangeError',
            message: 'The value of "offset" is out of range. ' +
                     `It must be >= 0 and <= ${8 - i}. Received ${offset}`
          });
      });

      [NaN, 1.01].forEach((offset) => {
        assert.throws(
          () => data[fn](val - 1, offset, i),
          {
            code: 'ERR_OUT_OF_RANGE',
            name: 'RangeError',
            message: 'The value of "offset" is out of range. ' +
                     `It must be an integer. Received ${offset}`
          });
      });
    });

    val *= 0x100;
  }
}

for (const fn of [
  'UInt8', 'UInt16LE', 'UInt16BE', 'UInt32LE', 'UInt32BE', 'UIntLE', 'UIntBE',
  'BigUInt64LE', 'BigUInt64BE',
]) {
  const p = Buffer.prototype;
  const lowerFn = fn.replace(/UInt/, 'Uint');
  assert.strictEqual(p[`write${fn}`], p[`write${lowerFn}`]);
  assert.strictEqual(p[`read${fn}`], p[`read${lowerFn}`]);
}
