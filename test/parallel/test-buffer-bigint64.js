'use strict';
require('../common');
const assert = require('assert');

const buf = Buffer.allocUnsafe(8);

['LE', 'BE'].forEach(function(endianness) {
  // should allow simple BigInts to be written and read
  let val = 123456789n;
  buf['writeBigInt64' + endianness](val, 0);
  let rtn = buf['readBigInt64' + endianness](0);
  assert.strictEqual(val, rtn);

  // should allow INT64_MAX to be written and read
  val = 9223372036854775807n;
  buf['writeBigInt64' + endianness](val, 0);
  rtn = buf['readBigInt64' + endianness](0);
  assert.strictEqual(val, rtn);

  // should read and write a negative signed 64-bit integer
  val = -123456789n;
  buf['writeBigInt64' + endianness](val, 0);
  assert.strictEqual(val, buf['readBigInt64' + endianness](0));

  // should read and write an unsigned 64-bit integer
  val = 123456789n;
  buf['writeBigUInt64' + endianness](val, 0);
  assert.strictEqual(val, buf['readBigUInt64' + endianness](0));

  // should throw a RangeError upon INT64_MAX+1 being written
  assert.throws(function() {
    const val = 9223372036854775808n;
    buf['writeBigInt64' + endianness](val, 0);
  }, RangeError);

  // should throw a RangeError upon UINT64_MAX+1 being written
  assert.throws(function() {
    const val = 18446744073709551616n;
    buf['writeBigUInt64' + endianness](val, 0);
  }, RangeError);

  // should throw a TypeError upon invalid input
  assert.throws(function() {
    buf['writeBigInt64' + endianness]('bad', 0);
  }, TypeError);

  // should throw a TypeError upon invalid input
  assert.throws(function() {
    buf['writeBigUInt64' + endianness]('bad', 0);
  }, TypeError);
});
