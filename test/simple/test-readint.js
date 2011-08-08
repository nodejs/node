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
  ASSERT.equal(0x23, data.readInt8(0, true));
  ASSERT.equal(0x23, data.readInt8(0, false));

  data[0] = 0xff;
  ASSERT.equal(-1, data.readInt8(0, true));
  ASSERT.equal(-1, data.readInt8(0, false));

  data[0] = 0x87;
  data[1] = 0xab;
  data[2] = 0x7c;
  data[3] = 0xef;
  ASSERT.equal(-121, data.readInt8(0, true));
  ASSERT.equal(-85, data.readInt8(1, true));
  ASSERT.equal(124, data.readInt8(2, true));
  ASSERT.equal(-17, data.readInt8(3, true));
  ASSERT.equal(-121, data.readInt8(0, false));
  ASSERT.equal(-85, data.readInt8(1, false));
  ASSERT.equal(124, data.readInt8(2, false));
  ASSERT.equal(-17, data.readInt8(3, false));
}


function test16() {
  var buffer = new Buffer(6);
  buffer[0] = 0x16;
  buffer[1] = 0x79;
  ASSERT.equal(0x1679, buffer.readInt16(0, true));
  ASSERT.equal(0x7916, buffer.readInt16(0, false));

  buffer[0] = 0xff;
  buffer[1] = 0x80;
  ASSERT.equal(-128, buffer.readInt16(0, true));
  ASSERT.equal(-32513, buffer.readInt16(0, false));

  /* test offset with weenix */
  buffer[0] = 0x77;
  buffer[1] = 0x65;
  buffer[2] = 0x65;
  buffer[3] = 0x6e;
  buffer[4] = 0x69;
  buffer[5] = 0x78;
  ASSERT.equal(0x7765, buffer.readInt16(0, true));
  ASSERT.equal(0x6565, buffer.readInt16(1, true));
  ASSERT.equal(0x656e, buffer.readInt16(2, true));
  ASSERT.equal(0x6e69, buffer.readInt16(3, true));
  ASSERT.equal(0x6978, buffer.readInt16(4, true));
  ASSERT.equal(0x6577, buffer.readInt16(0, false));
  ASSERT.equal(0x6565, buffer.readInt16(1, false));
  ASSERT.equal(0x6e65, buffer.readInt16(2, false));
  ASSERT.equal(0x696e, buffer.readInt16(3, false));
  ASSERT.equal(0x7869, buffer.readInt16(4, false));
}


function test32() {
  var buffer = new Buffer(6);
  buffer[0] = 0x43;
  buffer[1] = 0x53;
  buffer[2] = 0x16;
  buffer[3] = 0x79;
  ASSERT.equal(0x43531679, buffer.readInt32(0, true));
  ASSERT.equal(0x79165343, buffer.readInt32(0, false));

  buffer[0] = 0xff;
  buffer[1] = 0xfe;
  buffer[2] = 0xef;
  buffer[3] = 0xfa;
  ASSERT.equal(-69638, buffer.readInt32(0, true));
  ASSERT.equal(-84934913, buffer.readInt32(0, false));

  buffer[0] = 0x42;
  buffer[1] = 0xc3;
  buffer[2] = 0x95;
  buffer[3] = 0xa9;
  buffer[4] = 0x36;
  buffer[5] = 0x17;
  ASSERT.equal(0x42c395a9, buffer.readInt32(0, true));
  ASSERT.equal(-1013601994, buffer.readInt32(1, true));
  ASSERT.equal(-1784072681, buffer.readInt32(2, true));
  ASSERT.equal(-1449802942, buffer.readInt32(0, false));
  ASSERT.equal(917083587, buffer.readInt32(1, false));
  ASSERT.equal(389458325, buffer.readInt32(2, false));
}


test8();
test16();
test32();
