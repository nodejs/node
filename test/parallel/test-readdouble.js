'use strict';
/*
 * Tests to verify we're reading in doubles correctly
 */
require('../common');
const assert = require('assert');

/*
 * Test (64 bit) double
 */
function test(clazz) {
  var buffer = new clazz(8);

  buffer[0] = 0x55;
  buffer[1] = 0x55;
  buffer[2] = 0x55;
  buffer[3] = 0x55;
  buffer[4] = 0x55;
  buffer[5] = 0x55;
  buffer[6] = 0xd5;
  buffer[7] = 0x3f;
  assert.equal(1.1945305291680097e+103, buffer.readDoubleBE(0));
  assert.equal(0.3333333333333333, buffer.readDoubleLE(0));

  buffer[0] = 1;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = 0;
  buffer[5] = 0;
  buffer[6] = 0xf0;
  buffer[7] = 0x3f;
  assert.equal(7.291122019655968e-304, buffer.readDoubleBE(0));
  assert.equal(1.0000000000000002, buffer.readDoubleLE(0));

  buffer[0] = 2;
  assert.equal(4.778309726801735e-299, buffer.readDoubleBE(0));
  assert.equal(1.0000000000000004, buffer.readDoubleLE(0));

  buffer[0] = 1;
  buffer[6] = 0;
  buffer[7] = 0;
  assert.equal(7.291122019556398e-304, buffer.readDoubleBE(0));
  assert.equal(5e-324, buffer.readDoubleLE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xff;
  buffer[2] = 0xff;
  buffer[3] = 0xff;
  buffer[4] = 0xff;
  buffer[5] = 0xff;
  buffer[6] = 0x0f;
  buffer[7] = 0x00;
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.equal(2.225073858507201e-308, buffer.readDoubleLE(0));

  buffer[6] = 0xef;
  buffer[7] = 0x7f;
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.equal(1.7976931348623157e+308, buffer.readDoubleLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  buffer[4] = 0;
  buffer[5] = 0;
  buffer[6] = 0xf0;
  buffer[7] = 0x3f;
  assert.equal(3.03865e-319, buffer.readDoubleBE(0));
  assert.equal(1, buffer.readDoubleLE(0));

  buffer[6] = 0;
  buffer[7] = 0x40;
  assert.equal(3.16e-322, buffer.readDoubleBE(0));
  assert.equal(2, buffer.readDoubleLE(0));

  buffer[7] = 0xc0;
  assert.equal(9.5e-322, buffer.readDoubleBE(0));
  assert.equal(-2, buffer.readDoubleLE(0));

  buffer[6] = 0x10;
  buffer[7] = 0;
  assert.equal(2.0237e-320, buffer.readDoubleBE(0));
  assert.equal(2.2250738585072014e-308, buffer.readDoubleLE(0));

  buffer[6] = 0;
  assert.equal(0, buffer.readDoubleBE(0));
  assert.equal(0, buffer.readDoubleLE(0));
  assert.equal(false, 1 / buffer.readDoubleLE(0) < 0);

  buffer[7] = 0x80;
  assert.equal(6.3e-322, buffer.readDoubleBE(0));
  assert.equal(0, buffer.readDoubleLE(0));
  assert.equal(true, 1 / buffer.readDoubleLE(0) < 0);

  buffer[6] = 0xf0;
  buffer[7] = 0x7f;
  assert.equal(3.0418e-319, buffer.readDoubleBE(0));
  assert.equal(Infinity, buffer.readDoubleLE(0));

  buffer[6] = 0xf0;
  buffer[7] = 0xff;
  assert.equal(3.04814e-319, buffer.readDoubleBE(0));
  assert.equal(-Infinity, buffer.readDoubleLE(0));
}


test(Buffer);
