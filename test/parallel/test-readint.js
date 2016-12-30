'use strict';
/*
 * Tests to verify we're reading in signed integers correctly
 */
require('../common');
const assert = require('assert');

/*
 * Test 8 bit signed integers
 */
function test8(clazz) {
  var data = new clazz(4);

  data[0] = 0x23;
  assert.equal(0x23, data.readInt8(0));

  data[0] = 0xff;
  assert.equal(-1, data.readInt8(0));

  data[0] = 0x87;
  data[1] = 0xab;
  data[2] = 0x7c;
  data[3] = 0xef;
  assert.equal(-121, data.readInt8(0));
  assert.equal(-85, data.readInt8(1));
  assert.equal(124, data.readInt8(2));
  assert.equal(-17, data.readInt8(3));
}


function test16(clazz) {
  var buffer = new clazz(6);

  buffer[0] = 0x16;
  buffer[1] = 0x79;
  assert.equal(0x1679, buffer.readInt16BE(0));
  assert.equal(0x7916, buffer.readInt16LE(0));

  buffer[0] = 0xff;
  buffer[1] = 0x80;
  assert.equal(-128, buffer.readInt16BE(0));
  assert.equal(-32513, buffer.readInt16LE(0));

  /* test offset with weenix */
  buffer[0] = 0x77;
  buffer[1] = 0x65;
  buffer[2] = 0x65;
  buffer[3] = 0x6e;
  buffer[4] = 0x69;
  buffer[5] = 0x78;
  assert.equal(0x7765, buffer.readInt16BE(0));
  assert.equal(0x6565, buffer.readInt16BE(1));
  assert.equal(0x656e, buffer.readInt16BE(2));
  assert.equal(0x6e69, buffer.readInt16BE(3));
  assert.equal(0x6978, buffer.readInt16BE(4));
  assert.equal(0x6577, buffer.readInt16LE(0));
  assert.equal(0x6565, buffer.readInt16LE(1));
  assert.equal(0x6e65, buffer.readInt16LE(2));
  assert.equal(0x696e, buffer.readInt16LE(3));
  assert.equal(0x7869, buffer.readInt16LE(4));
}


function test32(clazz) {
  var buffer = new clazz(6);

  buffer[0] = 0x43;
  buffer[1] = 0x53;
  buffer[2] = 0x16;
  buffer[3] = 0x79;
  assert.equal(0x43531679, buffer.readInt32BE(0));
  assert.equal(0x79165343, buffer.readInt32LE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xfe;
  buffer[2] = 0xef;
  buffer[3] = 0xfa;
  assert.equal(-69638, buffer.readInt32BE(0));
  assert.equal(-84934913, buffer.readInt32LE(0));

  buffer[0] = 0x42;
  buffer[1] = 0xc3;
  buffer[2] = 0x95;
  buffer[3] = 0xa9;
  buffer[4] = 0x36;
  buffer[5] = 0x17;
  assert.equal(0x42c395a9, buffer.readInt32BE(0));
  assert.equal(-1013601994, buffer.readInt32BE(1));
  assert.equal(-1784072681, buffer.readInt32BE(2));
  assert.equal(-1449802942, buffer.readInt32LE(0));
  assert.equal(917083587, buffer.readInt32LE(1));
  assert.equal(389458325, buffer.readInt32LE(2));
}


test8(Buffer);
test16(Buffer);
test32(Buffer);
