/*
 * Tests to verify we're writing signed integers correctly
 */
var ASSERT = require('assert');

function test8() {
  var buffer = new Buffer(4);
  buffer.writeInt8(0x23, 0, 'big');
  buffer.writeInt8(0x23, 1, 'little');
  buffer.writeInt8(-5, 2, 'big');
  buffer.writeInt8(-5, 3, 'little');

  ASSERT.equal(0x23, buffer[0]);
  ASSERT.equal(0x23, buffer[1]);
  ASSERT.equal(0xfb, buffer[2]);
  ASSERT.equal(0xfb, buffer[3]);

  /* Make sure we handle truncation correctly */
  ASSERT.throws(function() {
    buffer.writeInt8(0xabc, 0, 'big');
  });
  ASSERT.throws(function() {
    buffer.writeInt8(0xabc, 0, 'little');
  });
}


function test16() {
  var buffer = new Buffer(6);
  buffer.writeInt16(0x0023, 0, 'big');
  buffer.writeInt16(0x0023, 2, 'little');
  ASSERT.equal(0x00, buffer[0]);
  ASSERT.equal(0x23, buffer[1]);
  ASSERT.equal(0x23, buffer[2]);
  ASSERT.equal(0x00, buffer[3]);
  buffer.writeInt16(-5, 0, 'big');
  buffer.writeInt16(-5, 2, 'little');
  ASSERT.equal(0xff, buffer[0]);
  ASSERT.equal(0xfb, buffer[1]);
  ASSERT.equal(0xfb, buffer[2]);
  ASSERT.equal(0xff, buffer[3]);

  buffer.writeInt16(-1679, 1, 'big');
  buffer.writeInt16(-1679, 3, 'little');
  ASSERT.equal(0xf9, buffer[1]);
  ASSERT.equal(0x71, buffer[2]);
  ASSERT.equal(0x71, buffer[3]);
  ASSERT.equal(0xf9, buffer[4]);
}


function test32() {
  var buffer = new Buffer(8);
  buffer.writeInt32(0x23, 0, 'big');
  buffer.writeInt32(0x23, 4, 'little');
  ASSERT.equal(0x00, buffer[0]);
  ASSERT.equal(0x00, buffer[1]);
  ASSERT.equal(0x00, buffer[2]);
  ASSERT.equal(0x23, buffer[3]);
  ASSERT.equal(0x23, buffer[4]);
  ASSERT.equal(0x00, buffer[5]);
  ASSERT.equal(0x00, buffer[6]);
  ASSERT.equal(0x00, buffer[7]);

  buffer.writeInt32(-5, 0, 'big');
  buffer.writeInt32(-5, 4, 'little');
  ASSERT.equal(0xff, buffer[0]);
  ASSERT.equal(0xff, buffer[1]);
  ASSERT.equal(0xff, buffer[2]);
  ASSERT.equal(0xfb, buffer[3]);
  ASSERT.equal(0xfb, buffer[4]);
  ASSERT.equal(0xff, buffer[5]);
  ASSERT.equal(0xff, buffer[6]);
  ASSERT.equal(0xff, buffer[7]);

  buffer.writeInt32(-805306713, 0, 'big');
  buffer.writeInt32(-805306713, 4, 'little');
  ASSERT.equal(0xcf, buffer[0]);
  ASSERT.equal(0xff, buffer[1]);
  ASSERT.equal(0xfe, buffer[2]);
  ASSERT.equal(0xa7, buffer[3]);
  ASSERT.equal(0xa7, buffer[4]);
  ASSERT.equal(0xfe, buffer[5]);
  ASSERT.equal(0xff, buffer[6]);
  ASSERT.equal(0xcf, buffer[7]);
}


test8();
test16();
test32();
