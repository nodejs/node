/*
 * Tests to verify we're writing floats correctly
 */
var ASSERT = require('assert');

function test() {
  var buffer = new Buffer(8);
  buffer.writeFloat(1, 0, 'big');
  buffer.writeFloat(1, 4, 'little');
  ASSERT.equal(0x3f, buffer[0]);
  ASSERT.equal(0x80, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x80, buffer[6]);
  ASSERT.equal(0x3f, buffer[7]);

  buffer.writeFloat(1.793662034335766e-43, 0, 'big');
  buffer.writeFloat(1.793662034335766e-43, 4, 'little');
  ASSERT.equal(0x00, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x80, buffer[3]);
  ASSERT.equal(0x80, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x00, buffer[7]);

  buffer.writeFloat(1/3, 0, 'big');
  buffer.writeFloat(1/3, 4, 'little');
  ASSERT.equal(0x3e, buffer[0]);
  ASSERT.equal(0xaa, buffer[1]);
  ASSERT.equal(0xaa, buffer[2]);
  ASSERT.equal(0xab, buffer[3]);
  ASSERT.equal(0xab, buffer[4]);
  ASSERT.equal(0xaa, buffer[5]);
  ASSERT.equal(0xaa, buffer[6]);
  ASSERT.equal(0x3e, buffer[7]);

  buffer.writeFloat(3.4028234663852886e+38, 0, 'big');
  buffer.writeFloat(3.4028234663852886e+38, 4, 'little');
  ASSERT.equal(0x7f, buffer[0]);
  ASSERT.equal(0x7f, buffer[1]);
  ASSERT.equal(0xff, buffer[2]);
  ASSERT.equal(0xff, buffer[3]);
  ASSERT.equal(0xff, buffer[4]);
  ASSERT.equal(0xff, buffer[5]);
  ASSERT.equal(0x7f, buffer[6]);
  ASSERT.equal(0x7f, buffer[7]);

  buffer.writeFloat(0*-1, 0, 'big');
  buffer.writeFloat(0*-1, 4, 'little');
  ASSERT.equal(0x80, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  ASSERT.equal(0x00, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x80, buffer[7]);
}


test();
