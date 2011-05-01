/*
 * A battery of tests to help us read a series of uints
 */

var ASSERT = require('assert');

/*
 * We need to check the following things:
 *  - We are correctly resolving big endian (doesn't mean anything for 8 bit)
 *  - Correctly resolving little endian (doesn't mean anything for 8 bit)
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test8() {
  var data = new Buffer(4);
  data[0] = 23;
  data[1] = 23;
  data[2] = 23;
  data[3] = 23;
  ASSERT.equal(23, data.readUInt8(0, 'big'));
  ASSERT.equal(23, data.readUInt8(0, 'little'));
  ASSERT.equal(23, data.readUInt8(1, 'big'));
  ASSERT.equal(23, data.readUInt8(1, 'little'));
  ASSERT.equal(23, data.readUInt8(2, 'big'));
  ASSERT.equal(23, data.readUInt8(2, 'little'));
  ASSERT.equal(23, data.readUInt8(3, 'big'));
  ASSERT.equal(23, data.readUInt8(3, 'little'));
  data[0] = 255; /* If it became a signed int, would be -1 */
  ASSERT.equal(255, data.readUInt8(0, 'big'));
  ASSERT.equal(255, data.readUInt8(0, 'little'));
}


/*
 * Test 16 bit unsigned integers. We need to verify the same set as 8 bit, only
 * now some of the issues actually matter:
 *  - We are correctly resolving big endian
 *  - Correctly resolving little endian
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test16() {
  var data = new Buffer(4);

  data[0] = 0;
  data[1] = 0x23;
  data[2] = 0x42;
  data[3] = 0x3f;

  ASSERT.equal(0x23, data.readUInt16(0, 'big'));
  ASSERT.equal(0x2342, data.readUInt16(1, 'big'));
  ASSERT.equal(0x423f, data.readUInt16(2, 'big'));

  ASSERT.equal(0x2300, data.readUInt16(0, 'little'));
  ASSERT.equal(0x4223, data.readUInt16(1, 'little'));
  ASSERT.equal(0x3f42, data.readUInt16(2, 'little'));

  data[0] = 0xfe;
  data[1] = 0xfe;

  ASSERT.equal(0xfefe, data.readUInt16(0, 'big'));
  ASSERT.equal(0xfefe, data.readUInt16(0, 'little'));
}


/*
 * Test 32 bit unsigned integers. We need to verify the same set as 8 bit, only
 * now some of the issues actually matter:
 *  - We are correctly resolving big endian
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test32() {
  var data = new Buffer(8);
  data[0] = 0x32;
  data[1] = 0x65;
  data[2] = 0x42;
  data[3] = 0x56;
  data[4] = 0x23;
  data[5] = 0xff;

  ASSERT.equal(0x32654256, data.readUInt32(0, 'big'));
  ASSERT.equal(0x65425623, data.readUInt32(1, 'big'));
  ASSERT.equal(0x425623ff, data.readUInt32(2, 'big'));

  ASSERT.equal(0x56426532, data.readUInt32(0, 'little'));
  ASSERT.equal(0x23564265, data.readUInt32(1, 'little'));
  ASSERT.equal(0xff235642, data.readUInt32(2, 'little'));
}


test8();
test16();
test32();
