/*
 * Tests to verify we're writing floats correctly
 */
var common = require('../common');
var ASSERT = require('assert');

function test(clazz) {
  var buffer = new clazz(8);

  buffer.writeFloatBE(1, 0);
  buffer.writeFloatLE(1, 4);
  ASSERT.equal(0x3f, buffer[0]);
  ASSERT.equal(0x80, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x80, buffer[6]);
  ASSERT.equal(0x3f, buffer[7]);

  buffer.writeFloatBE(1 / 3, 0);
  buffer.writeFloatLE(1 / 3, 4);
  ASSERT.equal(0x3e, buffer[0]);
  ASSERT.equal(0xaa, buffer[1]);
  ASSERT.equal(0xaa, buffer[2]);
  ASSERT.equal(0xab, buffer[3]);
  ASSERT.equal(0xab, buffer[4]);
  ASSERT.equal(0xaa, buffer[5]);
  ASSERT.equal(0xaa, buffer[6]);
  ASSERT.equal(0x3e, buffer[7]);

  buffer.writeFloatBE(3.4028234663852886e+38, 0);
  buffer.writeFloatLE(3.4028234663852886e+38, 4);
  ASSERT.equal(0x7f, buffer[0]);
  ASSERT.equal(0x7f, buffer[1]);
  ASSERT.equal(0xff, buffer[2]);
  ASSERT.equal(0xff, buffer[3]);
  ASSERT.equal(0xff, buffer[4]);
  ASSERT.equal(0xff, buffer[5]);
  ASSERT.equal(0x7f, buffer[6]);
  ASSERT.equal(0x7f, buffer[7]);

  buffer.writeFloatLE(1.1754943508222875e-38, 0);
  buffer.writeFloatBE(1.1754943508222875e-38, 4);
  ASSERT.equal(0x00, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x80, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x80, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x00, buffer[7]);

  buffer.writeFloatBE(0 * -1, 0);
  buffer.writeFloatLE(0 * -1, 4);
  ASSERT.equal(0x80, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x80, buffer[7]);

  buffer.writeFloatBE(Infinity, 0);
  buffer.writeFloatLE(Infinity, 4);
  ASSERT.equal(0x7F, buffer[0]);
  ASSERT.equal(0x80, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x80, buffer[6]);
  ASSERT.equal(0x7F, buffer[7]);
  ASSERT.equal(Infinity, buffer.readFloatBE(0));
  ASSERT.equal(Infinity, buffer.readFloatLE(4));

  buffer.writeFloatBE(-Infinity, 0);
  buffer.writeFloatLE(-Infinity, 4);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  ASSERT(0xFF === buffer[0] || 0x7F === buffer[0]);
  ASSERT.equal(0x80, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x80, buffer[6]);
  ASSERT.equal(0xFF, buffer[7]);
  ASSERT.equal(-Infinity, buffer.readFloatBE(0));
  ASSERT.equal(-Infinity, buffer.readFloatLE(4));

  buffer.writeFloatBE(NaN, 0);
  buffer.writeFloatLE(NaN, 4);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  ASSERT(0x7F === buffer[0] || 0xFF === buffer[0]);
  // mips processors use a slightly different NaN
  ASSERT(0xC0 === buffer[1] || 0xBF === buffer[1]);
  ASSERT(0x00 === buffer[2] || 0xFF === buffer[2]);
  ASSERT(0x00 === buffer[3] || 0xFF === buffer[3]);
  ASSERT(0x00 === buffer[4] || 0xFF === buffer[4]);
  ASSERT(0x00 === buffer[5] || 0xFF === buffer[5]);
  ASSERT(0xC0 === buffer[6] || 0xBF === buffer[6]);
  // Darwin ia32 does the other kind of NaN.
  // Compiler bug.  No one really cares.
  ASSERT(0x7F === buffer[7] || 0xFF === buffer[7]);
  ASSERT.ok(isNaN(buffer.readFloatBE(0)));
  ASSERT.ok(isNaN(buffer.readFloatLE(4)));
}


test(Buffer);
