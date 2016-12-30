'use strict';
/*
 * Tests to verify we're writing doubles correctly
 */
require('../common');
const assert = require('assert');

function test(clazz) {
  var buffer = new clazz(16);

  buffer.writeDoubleBE(2.225073858507201e-308, 0);
  buffer.writeDoubleLE(2.225073858507201e-308, 8);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x0f, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0xff, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xff, buffer[7]);
  assert.equal(0xff, buffer[8]);
  assert.equal(0xff, buffer[9]);
  assert.equal(0xff, buffer[10]);
  assert.equal(0xff, buffer[11]);
  assert.equal(0xff, buffer[12]);
  assert.equal(0xff, buffer[13]);
  assert.equal(0x0f, buffer[14]);
  assert.equal(0x00, buffer[15]);

  buffer.writeDoubleBE(1.0000000000000004, 0);
  buffer.writeDoubleLE(1.0000000000000004, 8);
  assert.equal(0x3f, buffer[0]);
  assert.equal(0xf0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x02, buffer[7]);
  assert.equal(0x02, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xf0, buffer[14]);
  assert.equal(0x3f, buffer[15]);

  buffer.writeDoubleBE(-2, 0);
  buffer.writeDoubleLE(-2, 8);
  assert.equal(0xc0, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0x00, buffer[14]);
  assert.equal(0xc0, buffer[15]);

  buffer.writeDoubleBE(1.7976931348623157e+308, 0);
  buffer.writeDoubleLE(1.7976931348623157e+308, 8);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0xef, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0xff, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xff, buffer[7]);
  assert.equal(0xff, buffer[8]);
  assert.equal(0xff, buffer[9]);
  assert.equal(0xff, buffer[10]);
  assert.equal(0xff, buffer[11]);
  assert.equal(0xff, buffer[12]);
  assert.equal(0xff, buffer[13]);
  assert.equal(0xef, buffer[14]);
  assert.equal(0x7f, buffer[15]);

  buffer.writeDoubleBE(0 * -1, 0);
  buffer.writeDoubleLE(0 * -1, 8);
  assert.equal(0x80, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0x00, buffer[14]);
  assert.equal(0x80, buffer[15]);

  buffer.writeDoubleBE(Infinity, 0);
  buffer.writeDoubleLE(Infinity, 8);
  assert.equal(0x7F, buffer[0]);
  assert.equal(0xF0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xF0, buffer[14]);
  assert.equal(0x7F, buffer[15]);
  assert.equal(Infinity, buffer.readDoubleBE(0));
  assert.equal(Infinity, buffer.readDoubleLE(8));

  buffer.writeDoubleBE(-Infinity, 0);
  buffer.writeDoubleLE(-Infinity, 8);
  assert.equal(0xFF, buffer[0]);
  assert.equal(0xF0, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.equal(0x00, buffer[8]);
  assert.equal(0x00, buffer[9]);
  assert.equal(0x00, buffer[10]);
  assert.equal(0x00, buffer[11]);
  assert.equal(0x00, buffer[12]);
  assert.equal(0x00, buffer[13]);
  assert.equal(0xF0, buffer[14]);
  assert.equal(0xFF, buffer[15]);
  assert.equal(-Infinity, buffer.readDoubleBE(0));
  assert.equal(-Infinity, buffer.readDoubleLE(8));

  buffer.writeDoubleBE(NaN, 0);
  buffer.writeDoubleLE(NaN, 8);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert(0x7F === buffer[0] || 0xFF === buffer[0]);
  // mips processors use a slightly different NaN
  assert(0xF8 === buffer[1] || 0xF7 === buffer[1]);
  assert(0x00 === buffer[2] || 0xFF === buffer[2]);
  assert(0x00 === buffer[3] || 0xFF === buffer[3]);
  assert(0x00 === buffer[4] || 0xFF === buffer[4]);
  assert(0x00 === buffer[5] || 0xFF === buffer[5]);
  assert(0x00 === buffer[6] || 0xFF === buffer[6]);
  assert(0x00 === buffer[7] || 0xFF === buffer[7]);
  assert(0x00 === buffer[8] || 0xFF === buffer[8]);
  assert(0x00 === buffer[9] || 0xFF === buffer[9]);
  assert(0x00 === buffer[10] || 0xFF === buffer[10]);
  assert(0x00 === buffer[11] || 0xFF === buffer[11]);
  assert(0x00 === buffer[12] || 0xFF === buffer[12]);
  assert(0x00 === buffer[13] || 0xFF === buffer[13]);
  assert(0xF8 === buffer[14] || 0xF7 === buffer[14]);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  assert(0x7F === buffer[15] || 0xFF === buffer[15]);
  assert.ok(isNaN(buffer.readDoubleBE(0)));
  assert.ok(isNaN(buffer.readDoubleLE(8)));
}


test(Buffer);
