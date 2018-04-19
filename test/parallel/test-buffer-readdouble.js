'use strict';

require('../common');
const assert = require('assert');

// Test (64 bit) double
const buffer = Buffer.allocUnsafe(8);

buffer[0] = 0x55;
buffer[1] = 0x55;
buffer[2] = 0x55;
buffer[3] = 0x55;
buffer[4] = 0x55;
buffer[5] = 0x55;
buffer[6] = 0xd5;
buffer[7] = 0x3f;
assert.strictEqual(buffer.readDoubleBE(0), 1.1945305291680097e+103);
assert.strictEqual(buffer.readDoubleLE(0), 0.3333333333333333);

buffer[0] = 1;
buffer[1] = 0;
buffer[2] = 0;
buffer[3] = 0;
buffer[4] = 0;
buffer[5] = 0;
buffer[6] = 0xf0;
buffer[7] = 0x3f;
assert.strictEqual(buffer.readDoubleBE(0), 7.291122019655968e-304);
assert.strictEqual(buffer.readDoubleLE(0), 1.0000000000000002);

buffer[0] = 2;
assert.strictEqual(buffer.readDoubleBE(0), 4.778309726801735e-299);
assert.strictEqual(buffer.readDoubleLE(0), 1.0000000000000004);

buffer[0] = 1;
buffer[6] = 0;
buffer[7] = 0;
assert.strictEqual(buffer.readDoubleBE(0), 7.291122019556398e-304);
assert.strictEqual(buffer.readDoubleLE(0), 5e-324);

buffer[0] = 0xff;
buffer[1] = 0xff;
buffer[2] = 0xff;
buffer[3] = 0xff;
buffer[4] = 0xff;
buffer[5] = 0xff;
buffer[6] = 0x0f;
buffer[7] = 0x00;
assert.ok(Number.isNaN(buffer.readDoubleBE(0)));
assert.strictEqual(buffer.readDoubleLE(0), 2.225073858507201e-308);

buffer[6] = 0xef;
buffer[7] = 0x7f;
assert.ok(Number.isNaN(buffer.readDoubleBE(0)));
assert.strictEqual(buffer.readDoubleLE(0), 1.7976931348623157e+308);

buffer[0] = 0;
buffer[1] = 0;
buffer[2] = 0;
buffer[3] = 0;
buffer[4] = 0;
buffer[5] = 0;
buffer[6] = 0xf0;
buffer[7] = 0x3f;
assert.strictEqual(buffer.readDoubleBE(0), 3.03865e-319);
assert.strictEqual(buffer.readDoubleLE(0), 1);

buffer[6] = 0;
buffer[7] = 0x40;
assert.strictEqual(buffer.readDoubleBE(0), 3.16e-322);
assert.strictEqual(buffer.readDoubleLE(0), 2);

buffer[7] = 0xc0;
assert.strictEqual(buffer.readDoubleBE(0), 9.5e-322);
assert.strictEqual(buffer.readDoubleLE(0), -2);

buffer[6] = 0x10;
buffer[7] = 0;
assert.strictEqual(buffer.readDoubleBE(0), 2.0237e-320);
assert.strictEqual(buffer.readDoubleLE(0), 2.2250738585072014e-308);

buffer[6] = 0;
assert.strictEqual(buffer.readDoubleBE(0), 0);
assert.strictEqual(buffer.readDoubleLE(0), 0);
assert.ok(1 / buffer.readDoubleLE(0) >= 0);

buffer[7] = 0x80;
assert.strictEqual(buffer.readDoubleBE(0), 6.3e-322);
assert.strictEqual(buffer.readDoubleLE(0), -0);
assert.ok(1 / buffer.readDoubleLE(0) < 0);

buffer[6] = 0xf0;
buffer[7] = 0x7f;
assert.strictEqual(buffer.readDoubleBE(0), 3.0418e-319);
assert.strictEqual(buffer.readDoubleLE(0), Infinity);

buffer[7] = 0xff;
assert.strictEqual(buffer.readDoubleBE(0), 3.04814e-319);
assert.strictEqual(buffer.readDoubleLE(0), -Infinity);

['readDoubleLE', 'readDoubleBE'].forEach((fn) => {

  // Verify that default offset works fine.
  buffer[fn](undefined);
  buffer[fn]();

  ['', '0', null, {}, [], () => {}, true, false].forEach((off) => {
    assert.throws(
      () => buffer[fn](off),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  [Infinity, -1, 1].forEach((offset) => {
    assert.throws(
      () => buffer[fn](offset),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError [ERR_OUT_OF_RANGE]',
        message: 'The value of "offset" is out of range. ' +
                 `It must be >= 0 and <= 0. Received ${offset}`
      });
  });

  assert.throws(
    () => Buffer.alloc(1)[fn](1),
    {
      code: 'ERR_BUFFER_OUT_OF_BOUNDS',
      name: 'RangeError [ERR_BUFFER_OUT_OF_BOUNDS]',
      message: 'Attempt to write outside buffer bounds'
    });

  [NaN, 1.01].forEach((offset) => {
    assert.throws(
      () => buffer[fn](offset),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError [ERR_OUT_OF_RANGE]',
        message: 'The value of "offset" is out of range. ' +
                 `It must be an integer. Received ${offset}`
      });
  });
});
