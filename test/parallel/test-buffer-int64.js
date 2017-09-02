'use strict';
require('../common');
const assert = require('assert');

const Buffer = require('buffer').Buffer;

const buf = new Buffer(8);

['LE', 'BE'].forEach(function(endianness) {
  // should allow simple ints to be written and read
  let val = 123456789;
  buf['writeInt64' + endianness](val, 0);
  let rtn = buf['readInt64' + endianness](0);
  assert.strictEqual(val.toString(), rtn);

  // should allow INT64_MAX to be written and read
  val = '9223372036854775807';
  buf['writeInt64' + endianness](val, 0);
  rtn = buf['readInt64' + endianness](0);
  assert.strictEqual(val, rtn);

  // should read and write a negative signed 64-bit integer
  val = -123456789;
  buf['writeInt64' + endianness](val, 0);
  assert.strictEqual(val.toString(), buf['readInt64' + endianness](0));

  // should read and write an unsigned 64-bit integer
  val = 123456789;
  buf['writeUInt64' + endianness](val, 0);
  assert.strictEqual(val.toString(), buf['readUInt64' + endianness](0));

  // should throw a RangeError upon INT64_MAX+1 being written
  assert.throws(function() {
    const val = '9223372036854775808';
    buf['writeInt64' + endianness](val, 0);
  }, RangeError);

  // should throw a RangeError upon UINT64_MAX+1 being written
  assert.throws(function() {
    const val = '18446744073709551616';
    buf['writeUInt64' + endianness](val, 0);
  }, RangeError);

  // should throw a TypeError upon invalid input
  assert.throws(function() {
    buf['writeInt64' + endianness]('bad', 0);
  }, TypeError);

  // should throw a TypeError upon invalid input
  assert.throws(function() {
    buf['writeUInt64' + endianness]('bad', 0);
  }, TypeError);
});
