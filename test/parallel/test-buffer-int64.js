'use strict';
var common = require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer;

var buf = new Buffer(8);

['LE', 'BE'].forEach(function (endianness) {
  // should allow simple ints to be written and read
  var val = 123456789;
  buf['writeInt64' + endianness](val, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal(val, rtn);

  // should allow INT64_MAX to be written and read
  var val = '9223372036854775807';
  buf['writeInt64' + endianness](val, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal(val, rtn);

  // should return a Number when reading Number.MIN_SAFE_INTEGER
  buf['writeInt64' + endianness](Number.MIN_SAFE_INTEGER, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal('number', typeof rtn);
  assert.equal(Number.MIN_SAFE_INTEGER, rtn);

  // should return a Number when reading Number.MAX_SAFE_INTEGER
  buf['writeInt64' + endianness](Number.MAX_SAFE_INTEGER, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal('number', typeof rtn);
  assert.equal(Number.MAX_SAFE_INTEGER, rtn);

  // should return a String when reading Number.MAX_SAFE_INTEGER+1
  var plus_one = '9007199254740992';
  buf['writeInt64' + endianness](plus_one, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal('string', typeof rtn);
  assert.equal(plus_one, rtn);

  // should return a String when reading Number.MIN_SAFE_INTEGER-1
  var minus_one = '-9007199254740992';
  buf['writeInt64' + endianness](minus_one, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal('string', typeof rtn);
  assert.equal(minus_one, rtn);

  // should return a Number when reading 0, even when written as a String
  var zero = '0';
  buf['writeInt64' + endianness](zero, 0);
  var rtn = buf['readInt64' + endianness](0);
  assert.equal('number', typeof rtn);
  assert.equal(0, rtn);

  // should read and write a negative signed 64-bit integer
  var val = -123456789;
  buf['writeInt64' + endianness](val, 0);
  assert.equal(val, buf['readInt64' + endianness](0));

  // should read and write an unsigned 64-bit integer
  var val = 123456789;
  buf['writeUInt64' + endianness](val, 0);
  assert.equal(val, buf['readUInt64' + endianness](0));

  // should throw a RangeError upon INT64_MAX+1 being written
  assert.throws(function () {
    var val = '9223372036854775808';
    buf['writeInt64' + endianness](val, 0);
  }, RangeError);

  // should throw a RangeError upon UINT64_MAX+1 being written
  assert.throws(function () {
    var val = '18446744073709551616';
    buf['writeUInt64' + endianness](val, 0);
  }, RangeError);
});
