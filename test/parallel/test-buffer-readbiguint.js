'use strict';

require('../common');
const assert = require('assert');

// Test OOB
{
  const buffer = Buffer.alloc(16);
  ['Int64BE', 'Int64LE', 'Int128BE', 'Int128LE'].forEach((fn) => {
    const readfn = `readBigU${fn}`;

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

// Test 64 bit unsigned integers
{
  const buffer = Buffer.of(
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A);

  assert.strictEqual(buffer.readBigUInt64BE(0), 0x0807060504030201n);
  assert.strictEqual(buffer.readBigUInt64BE(1), 0x0908070605040302n);
  assert.strictEqual(buffer.readBigUInt64BE(2), 0x0A08070605040302n);

  assert.strictEqual(buffer.readBigUInt64LE(0), 0x0102030405060708n);
  assert.strictEqual(buffer.readBigUInt64LE(1), 0x0203040506070809n);
  assert.strictEqual(buffer.readBigUInt64LE(2), 0x030405060708090An);

  buffer[0] = 0x80;
  buffer[1] = 0x7F;
  buffer[2] = 0xA0;
  buffer[3] = 0x5F;
  buffer[4] = 0xC0;
  buffer[5] = 0x3F;
  buffer[6] = 0xE0;
  buffer[7] = 0x1F;

  assert.strictEqual(buffer.readBigUInt64BE(0), 9259295692004778015n);
  assert.strictEqual(buffer.readBigUInt64BE(1), 9196455718400564992n);
  assert.strictEqual(buffer.readBigUInt64BE(2), 11556166549431582720n);

  assert.strictEqual(buffer.readBigUInt64LE(0), 2296905905429577600n);
  assert.strictEqual(buffer.readBigUInt64LE(1), 8972288693084287n);
  assert.strictEqual(buffer.readBigUInt64LE(2), 35048002707360n);
}
// Test 128 bit unsigned integers
{
  const buffer = Buffer.of(
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x0F, 0xF0, 0x1F, 0xF1, 0x2E, 0xE2, 0xD3, 0x3D
  );

  assert.strictEqual(buffer.readBigUInt128BE(0),
                     0x00112233445566778899AABBCCDDEEFFn);
  assert.strictEqual(buffer.readBigUInt128BE(1),
                     0x112233445566778899AABBCCDDEEFF0Fn);
  assert.strictEqual(buffer.readBigUInt128BE(2),
                     0x2233445566778899AABBCCDDEEFF0FF0n);

  assert.strictEqual(buffer.readBigUInt128LE(0),
                     0xFFEEDDCCBBAA99887766554433221100n);
  assert.strictEqual(buffer.readBigUInt128LE(1),
                     0x0FFFEEDDCCBBAA998877665544332211n);
  assert.strictEqual(buffer.readBigUInt128LE(2),
                     0xF00FFFEEDDCCBBAA9988776655443322n);

  // Unsigned integers; no effect.
  buffer[0] = 0x80;
  buffer[7] = 0x80;

  assert.strictEqual(buffer.readBigUInt128BE(0),
                     88962710306127702866241727433142015);

  assert.strictEqual(buffer.readBigUInt128LE(0),
                     340193404210632335760508365704335069440);

  // Will likely need more.
}
