'use strict';
var common = require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer;

// TODO: use `sizeof.pointer` here when it's landed
var size = 8;

['LE', 'BE'].forEach(function (endianness) {
  var buf = new Buffer(size);

  // should write and read back a pointer (Buffer) in a Buffer with length
  var test = new Buffer('hello');
  buf['writePointer' + endianness](test, 0);
  var out = buf['readPointer' + endianness](0, test.length);
  assert.equal(out.length, test.length);
  assert.equal(out.address(), test.address());
  for (var i = 0, l = out.length; i < l; i++) {
    assert.equal(out[i], test[i]);
  }

  // should return `null` when reading a NULL pointer
  buf.fill(0);
  assert.strictEqual(null, buf['readPointer' + endianness](0));

  // should write a NULL pointer when `null` is given
  buf.fill(1);
  buf['writePointer' + endianness](null, 0);
  for (var i = 0, l = buf.length; i < l; i++) {
    assert.equal(buf[i], 0);
  }

  // should read two pointers next to each other in memory
  var buf = new Buffer(8 * 2);
  var a = new Buffer('hello');
  var b = new Buffer('world');
  buf['writePointer' + endianness](a, 0 * size);
  buf['writePointer' + endianness](b, 1 * size);
  var _a = buf['readPointer' + endianness](0 * size);
  var _b = buf['readPointer' + endianness](1 * size);
  assert.equal(a.address(), _a.address());
  assert.equal(b.address(), _b.address());
});
