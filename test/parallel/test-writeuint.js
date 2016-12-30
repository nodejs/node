'use strict';
/*
 * A battery of tests to help us read a series of uints
 */
require('../common');
const assert = require('assert');

/*
 * We need to check the following things:
 *  - We are correctly resolving big endian (doesn't mean anything for 8 bit)
 *  - Correctly resolving little endian (doesn't mean anything for 8 bit)
 *  - Correctly using the offsets
 *  - Correctly interpreting values that are beyond the signed range as unsigned
 */
function test8(clazz) {
  var data = new clazz(4);

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  assert.equal(23, data[0]);
  assert.equal(23, data[1]);
  assert.equal(23, data[2]);
  assert.equal(23, data[3]);

  data.writeUInt8(23, 0);
  data.writeUInt8(23, 1);
  data.writeUInt8(23, 2);
  data.writeUInt8(23, 3);
  assert.equal(23, data[0]);
  assert.equal(23, data[1]);
  assert.equal(23, data[2]);
  assert.equal(23, data[3]);

  data.writeUInt8(255, 0);
  assert.equal(255, data[0]);

  data.writeUInt8(255, 0);
  assert.equal(255, data[0]);
}


function test16(clazz) {
  var value = 0x2343;
  var data = new clazz(4);

  data.writeUInt16BE(value, 0);
  assert.equal(0x23, data[0]);
  assert.equal(0x43, data[1]);

  data.writeUInt16BE(value, 1);
  assert.equal(0x23, data[1]);
  assert.equal(0x43, data[2]);

  data.writeUInt16BE(value, 2);
  assert.equal(0x23, data[2]);
  assert.equal(0x43, data[3]);

  data.writeUInt16LE(value, 0);
  assert.equal(0x23, data[1]);
  assert.equal(0x43, data[0]);

  data.writeUInt16LE(value, 1);
  assert.equal(0x23, data[2]);
  assert.equal(0x43, data[1]);

  data.writeUInt16LE(value, 2);
  assert.equal(0x23, data[3]);
  assert.equal(0x43, data[2]);

  value = 0xff80;
  data.writeUInt16LE(value, 0);
  assert.equal(0xff, data[1]);
  assert.equal(0x80, data[0]);

  data.writeUInt16BE(value, 0);
  assert.equal(0xff, data[0]);
  assert.equal(0x80, data[1]);
}


function test32(clazz) {
  var data = new clazz(6);
  var value = 0xe7f90a6d;

  data.writeUInt32BE(value, 0);
  assert.equal(0xe7, data[0]);
  assert.equal(0xf9, data[1]);
  assert.equal(0x0a, data[2]);
  assert.equal(0x6d, data[3]);

  data.writeUInt32BE(value, 1);
  assert.equal(0xe7, data[1]);
  assert.equal(0xf9, data[2]);
  assert.equal(0x0a, data[3]);
  assert.equal(0x6d, data[4]);

  data.writeUInt32BE(value, 2);
  assert.equal(0xe7, data[2]);
  assert.equal(0xf9, data[3]);
  assert.equal(0x0a, data[4]);
  assert.equal(0x6d, data[5]);

  data.writeUInt32LE(value, 0);
  assert.equal(0xe7, data[3]);
  assert.equal(0xf9, data[2]);
  assert.equal(0x0a, data[1]);
  assert.equal(0x6d, data[0]);

  data.writeUInt32LE(value, 1);
  assert.equal(0xe7, data[4]);
  assert.equal(0xf9, data[3]);
  assert.equal(0x0a, data[2]);
  assert.equal(0x6d, data[1]);

  data.writeUInt32LE(value, 2);
  assert.equal(0xe7, data[5]);
  assert.equal(0xf9, data[4]);
  assert.equal(0x0a, data[3]);
  assert.equal(0x6d, data[2]);
}


function testUint(clazz) {
  const data = new clazz(8);
  var val = 1;

  // Test 0 to 5 bytes.
  for (var i = 0; i <= 5; i++) {
    const errmsg = `byteLength: ${i}`;
    assert.throws(function() {
      data.writeUIntBE(val, 0, i);
    }, /"value" argument is out of bounds/, errmsg);
    assert.throws(function() {
      data.writeUIntLE(val, 0, i);
    }, /"value" argument is out of bounds/, errmsg);
    val *= 0x100;
  }
}


test8(Buffer);
test16(Buffer);
test32(Buffer);
testUint(Buffer);
