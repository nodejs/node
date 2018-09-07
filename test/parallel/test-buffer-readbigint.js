'use strict';

require('../common');
const assert = require('assert');

// Test OOB
{
  const buffer = Buffer.alloc(16);
  ['Int64BE', 'Int64LE', 'Int128BE', 'Int128LE'].forEach((fn) => {
    const readfn = `readBig${fn}`;

    // Verify that the default offset works fine
    buffer[readfn](undefined);
    buffer[readfn]();

    ['', '0', null, {}, [], () => {}, true, false].forEach((o) => {
      assert.throws(
        () => buffer[readfn](o),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          name: 'TypeError [ERR_INVALID_ARG_TYPE]'
        });
    });

    [Infinity, -1, -4294967295].forEach((offset) => {
      assert.throws(
        () => buffer[readfn](offset),
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

// Test 64 bit signed integers
{
  const buffer = Buffer.of(
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A);

  assert.strictEqual(buffer.readBigInt64BE(0), 0x0807060504030201n);
  assert.strictEqual(buffer.readBigInt64BE(1), 0x0908070605040302n);
  assert.strictEqual(buffer.readBigInt64BE(2), 0x0A08070605040302n);

  assert.strictEqual(buffer.readBigInt64LE(0), 0x0102030405060708n);
  assert.strictEqual(buffer.readBigInt64LE(1), 0x0203040506070809n);
  assert.strictEqual(buffer.readBigInt64LE(2), 0x030405060708090An);

  buffer[0] = 0x80;
  buffer[1] = 0x7F;
  buffer[2] = 0xA0;
  buffer[3] = 0x5F;
  buffer[4] = 0xC0;
  buffer[5] = 0x3F;
  buffer[6] = 0xE0;
  buffer[7] = 0x1F;

  assert.strictEqual(buffer.readBigInt64BE(0), -9187448381704773601n);
  assert.strictEqual(buffer.readBigInt64BE(1), 9196455718400565001n);
  assert.strictEqual(buffer.readBigInt64BE(2), -6890577524277966582n);

  assert.strictEqual(buffer.readBigInt64LE(0), 2296905905429577600n);
  assert.strictEqual(buffer.readBigInt64LE(1), 657490635034435711n);
  assert.strictEqual(buffer.readBigInt64LE(2), 723144263172382624n);
}
// Test 128 bit signed integers
{
  const buffer = Buffer.of(
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x0F, 0xF0, 0x1F, 0xF1, 0x2E, 0xE2, 0xD3, 0x3D
  );

  assert.strictEqual(buffer.readBigInt128BE(0),
                     0x00112233445566778899AABBCCDDEEFFn);
  assert.strictEqual(buffer.readBigInt128BE(1),
                     0x112233445566778899AABBCCDDEEFF0Fn);
  assert.strictEqual(buffer.readBigInt128BE(2),
                     0x2233445566778899AABBCCDDEEFF0FF0n);

  assert.strictEqual(buffer.readBigInt128LE(0),
                     -0x0112233445566778899AABBCCDDEF00n);
  assert.strictEqual(buffer.readBigInt128LE(1),
                     0x0FFFEEDDCCBBAA998877665544332211n);
  assert.strictEqual(buffer.readBigInt128LE(2),
                     -0xFF000112233445566778899AABBCCDEn);

  // Definite negativity
  buffer[0] = 0x80;
  buffer[7] = 0x80;

  assert.strictEqual(buffer.readBigInt128BE(0),
                     -170052220750163103862800365325064999169n);

  assert.strictEqual(buffer.readBigInt128LE(0),
                     -88962710306127702217723381091790464n);

  // Will likely need more.
}
