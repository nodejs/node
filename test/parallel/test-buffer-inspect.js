'use strict';
var common = require('../common');
var assert = require('assert');

var util = require('util');

var buffer = require('buffer');

buffer.INSPECT_MAX_BYTES = 2;

var b = new Buffer(4);
b.fill('1234');

var s = new buffer.SlowBuffer(4);
s.fill('1234');

var expected = /<Buffer@0x[\da-f]+ 31 32 ... >/;

assert(expected.test(util.inspect(b)));
assert(expected.test(util.inspect(s)));

b = new Buffer(2);
b.fill('12');

s = new buffer.SlowBuffer(2);
s.fill('12');

expected = /<Buffer@0x[\da-f]+ 31 32>/;

assert(expected.test(util.inspect(b)));
assert(expected.test(util.inspect(s)));

buffer.INSPECT_MAX_BYTES = Infinity;

assert.doesNotThrow(function() {
  assert(expected.test(util.inspect(b)));
  assert(expected.test(util.inspect(s)));
});
