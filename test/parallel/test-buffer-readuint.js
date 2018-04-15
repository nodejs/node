'use strict';

require('../common');
const assert = require('assert');

// Test OOB
{
  const buffer = Buffer.alloc(4);

  ['UInt8', 'UInt16BE', 'UInt16LE', 'UInt32BE', 'UInt32LE'].forEach((fn) => {

    // Verify that default offset works fine.
    buffer[`read${fn}`](undefined);
    buffer[`read${fn}`]();

    ['', '0', null, {}, [], () => {}, true, false].forEach((o) => {
      assert.throws(
        () => buffer[`read${fn}`](o),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          name: 'TypeError [ERR_INVALID_ARG_TYPE]'
        });
    });

    [Infinity, -1, -4294967295].forEach((offset) => {
      assert.throws(
        () => buffer[`read${fn}`](offset),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError [ERR_OUT_OF_RANGE]'
        });
    });

    [NaN, 1.01].forEach((offset) => {
      assert.throws(
        () => buffer[`read${fn}`](offset),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError [ERR_OUT_OF_RANGE]',
          message: 'The value of "offset" is out of range. ' +
                   `It must be an integer. Received ${offset}`
        });
    });
  });
}

// Test 8 bit unsigned integers
{
  const data = Buffer.from([0xff, 0x2a, 0x2a, 0x2a]);
  assert.strictEqual(255, data.readUInt8(0));
  assert.strictEqual(42, data.readUInt8(1));
  assert.strictEqual(42, data.readUInt8(2));
  assert.strictEqual(42, data.readUInt8(3));
}

// Test 16 bit unsigned integers
{
  const data = Buffer.from([0x00, 0x2a, 0x42, 0x3f]);
  assert.strictEqual(0x2a, data.readUInt16BE(0));
  assert.strictEqual(0x2a42, data.readUInt16BE(1));
  assert.strictEqual(0x423f, data.readUInt16BE(2));
  assert.strictEqual(0x2a00, data.readUInt16LE(0));
  assert.strictEqual(0x422a, data.readUInt16LE(1));
  assert.strictEqual(0x3f42, data.readUInt16LE(2));

  data[0] = 0xfe;
  data[1] = 0xfe;
  assert.strictEqual(0xfefe, data.readUInt16BE(0));
  assert.strictEqual(0xfefe, data.readUInt16LE(0));
}

// Test 32 bit unsigned integers
{
  const data = Buffer.from([0x32, 0x65, 0x42, 0x56, 0x23, 0xff]);
  assert.strictEqual(0x32654256, data.readUInt32BE(0));
  assert.strictEqual(0x65425623, data.readUInt32BE(1));
  assert.strictEqual(0x425623ff, data.readUInt32BE(2));
  assert.strictEqual(0x56426532, data.readUInt32LE(0));
  assert.strictEqual(0x23564265, data.readUInt32LE(1));
  assert.strictEqual(0xff235642, data.readUInt32LE(2));
}

// Test UInt
{
  const buffer = Buffer.from([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]);

  assert.strictEqual(buffer.readUIntLE(0, 1), 0x01);
  assert.strictEqual(buffer.readUIntBE(0, 1), 0x01);
  assert.strictEqual(buffer.readUIntLE(0, 3), 0x030201);
  assert.strictEqual(buffer.readUIntBE(0, 3), 0x010203);
  assert.strictEqual(buffer.readUIntLE(0, 5), 0x0504030201);
  assert.strictEqual(buffer.readUIntBE(0, 5), 0x0102030405);
  assert.strictEqual(buffer.readUIntLE(0, 6), 0x060504030201);
  assert.strictEqual(buffer.readUIntBE(0, 6), 0x010203040506);
  assert.strictEqual(buffer.readUIntLE(1, 6), 0x070605040302);
  assert.strictEqual(buffer.readUIntBE(1, 6), 0x020304050607);
  assert.strictEqual(buffer.readUIntLE(2, 6), 0x080706050403);
  assert.strictEqual(buffer.readUIntBE(2, 6), 0x030405060708);

  // Check byteLength.
  ['readUIntBE', 'readUIntLE'].forEach((fn) => {
    ['', '0', null, {}, [], () => {}, true, false, undefined].forEach((len) => {
      assert.throws(
        () => buffer[fn](0, len),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [Infinity, -1].forEach((byteLength) => {
      assert.throws(
        () => buffer[fn](0, byteLength),
        {
          code: 'ERR_OUT_OF_RANGE',
          message: 'The value of "byteLength" is out of range. ' +
                   `It must be >= 1 and <= 6. Received ${byteLength}`
        });
    });

    [NaN, 1.01].forEach((byteLength) => {
      assert.throws(
        () => buffer[fn](0, byteLength),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError [ERR_OUT_OF_RANGE]',
          message: 'The value of "byteLength" is out of range. ' +
                   `It must be an integer. Received ${byteLength}`
        });
    });
  });

  // Test 1 to 6 bytes.
  for (let i = 1; i < 6; i++) {
    ['readUIntBE', 'readUIntLE'].forEach((fn) => {
      ['', '0', null, {}, [], () => {}, true, false, undefined].forEach((o) => {
        assert.throws(
          () => buffer[fn](o, i),
          {
            code: 'ERR_INVALID_ARG_TYPE',
            name: 'TypeError [ERR_INVALID_ARG_TYPE]'
          });
      });

      [Infinity, -1, -4294967295].forEach((offset) => {
        assert.throws(
          () => buffer[fn](offset, i),
          {
            code: 'ERR_OUT_OF_RANGE',
            name: 'RangeError [ERR_OUT_OF_RANGE]',
            message: 'The value of "offset" is out of range. ' +
                     `It must be >= 0 and <= ${8 - i}. Received ${offset}`
          });
      });

      [NaN, 1.01].forEach((offset) => {
        assert.throws(
          () => buffer[fn](offset, i),
          {
            code: 'ERR_OUT_OF_RANGE',
            name: 'RangeError [ERR_OUT_OF_RANGE]',
            message: 'The value of "offset" is out of range. ' +
                     `It must be an integer. Received ${offset}`
          });
      });
    });
  }
}
