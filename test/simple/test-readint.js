/*
 * Tests to verify we're reading in signed integers correctly
 */
var ASSERT = require('assert');

/*
 * Test 8 bit signed integers
 */
function test8() {
  var data = new Buffer(4);

  data[0] = 0x23;
  ASSERT.equal(0x23, data.readInt8(0, 'big'));
  ASSERT.equal(0x23, data.readInt8(0, 'little'));

  data[0] = 0xff;
  ASSERT.equal(-1, data.readInt8(0, 'big'));
  ASSERT.equal(-1, data.readInt8(0, 'little'));

  data[0] = 0x87;
  data[1] = 0xab;
  data[2] = 0x7c;
  data[3] = 0xef;
  ASSERT.equal(-121, data.readInt8(0, 'big'));
  ASSERT.equal(-85, data.readInt8(1, 'big'));
  ASSERT.equal(124, data.readInt8(2, 'big'));
  ASSERT.equal(-17, data.readInt8(3, 'big'));
  ASSERT.equal(-121, data.readInt8(0, 'little'));
  ASSERT.equal(-85, data.readInt8(1, 'little'));
  ASSERT.equal(124, data.readInt8(2, 'little'));
  ASSERT.equal(-17, data.readInt8(3, 'little'));
}


function test16() {
  var buffer = new Buffer(6);
  buffer[0] = 0x16;
  buffer[1] = 0x79;
  ASSERT.equal(0x1679, buffer.readInt16(0, 'big'));
  ASSERT.equal(0x7916, buffer.readInt16(0, 'little'));

  buffer[0] = 0xff;
  buffer[1] = 0x80;
  ASSERT.equal(-128, buffer.readInt16(0, 'big'));
  ASSERT.equal(-32513, buffer.readInt16(0, 'little'));

  /* test offset with weenix */
  buffer[0] = 0x77;
  buffer[1] = 0x65;
  buffer[2] = 0x65;
  buffer[3] = 0x6e;
  buffer[4] = 0x69;
  buffer[5] = 0x78;
  ASSERT.equal(0x7765, buffer.readInt16(0, 'big'));
  ASSERT.equal(0x6565, buffer.readInt16(1, 'big'));
  ASSERT.equal(0x656e, buffer.readInt16(2, 'big'));
  ASSERT.equal(0x6e69, buffer.readInt16(3, 'big'));
  ASSERT.equal(0x6978, buffer.readInt16(4, 'big'));
  ASSERT.equal(0x6577, buffer.readInt16(0, 'little'));
  ASSERT.equal(0x6565, buffer.readInt16(1, 'little'));
  ASSERT.equal(0x6e65, buffer.readInt16(2, 'little'));
  ASSERT.equal(0x696e, buffer.readInt16(3, 'little'));
  ASSERT.equal(0x7869, buffer.readInt16(4, 'little'));
}


function test32() {
  var buffer = new Buffer(6);
  buffer[0] = 0x43;
  buffer[1] = 0x53;
  buffer[2] = 0x16;
  buffer[3] = 0x79;
  ASSERT.equal(0x43531679, buffer.readInt32(0, 'big'));
  ASSERT.equal(0x79165343, buffer.readInt32(0, 'little'));

  buffer[0] = 0xff;
  buffer[1] = 0xfe;
  buffer[2] = 0xef;
  buffer[3] = 0xfa;
  ASSERT.equal(-69638, buffer.readInt32(0, 'big'));
  ASSERT.equal(-84934913, buffer.readInt32(0, 'little'));

  buffer[0] = 0x42;
  buffer[1] = 0xc3;
  buffer[2] = 0x95;
  buffer[3] = 0xa9;
  buffer[4] = 0x36;
  buffer[5] = 0x17;
  ASSERT.equal(0x42c395a9, buffer.readInt32(0, 'big'));
  ASSERT.equal(-1013601994, buffer.readInt32(1, 'big'));
  ASSERT.equal(-1784072681, buffer.readInt32(2, 'big'));
  ASSERT.equal(-1449802942, buffer.readInt32(0, 'little'));
  ASSERT.equal(917083587, buffer.readInt32(1, 'little'));
  ASSERT.equal(389458325, buffer.readInt32(2, 'little'));
}


test8();
test16();
test32();
