var common = require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer;

var b = new Buffer('abcdef');
var buf_a = new Buffer('a');
var buf_bc = new Buffer('bc');
var buf_f = new Buffer('f');
var buf_z = new Buffer('z');
var buf_empty = new Buffer('');

assert.equal(b.indexOf('a'), 0);
assert.equal(b.indexOf('a', 1), -1);
assert.equal(b.indexOf('a', -1), -1);
assert.equal(b.indexOf('a', -4), -1);
assert.equal(b.indexOf('a', -b.length), 0);
assert.equal(b.indexOf('a', NaN), 0);
assert.equal(b.indexOf('a', -Infinity), 0);
assert.equal(b.indexOf('a', Infinity), -1);
assert.equal(b.indexOf('bc'), 1);
assert.equal(b.indexOf('bc', 2), -1);
assert.equal(b.indexOf('bc', -1), -1);
assert.equal(b.indexOf('bc', -3), -1);
assert.equal(b.indexOf('bc', -5), 1);
assert.equal(b.indexOf('bc', NaN), 1);
assert.equal(b.indexOf('bc', -Infinity), 1);
assert.equal(b.indexOf('bc', Infinity), -1);
assert.equal(b.indexOf('f'), b.length - 1);
assert.equal(b.indexOf('z'), -1);
assert.equal(b.indexOf(''), -1);
assert.equal(b.indexOf('', 1), -1);
assert.equal(b.indexOf('', b.length + 1), -1);
assert.equal(b.indexOf('', Infinity), -1);
assert.equal(b.indexOf(buf_a), 0);
assert.equal(b.indexOf(buf_a, 1), -1);
assert.equal(b.indexOf(buf_a, -1), -1);
assert.equal(b.indexOf(buf_a, -4), -1);
assert.equal(b.indexOf(buf_a, -b.length), 0);
assert.equal(b.indexOf(buf_a, NaN), 0);
assert.equal(b.indexOf(buf_a, -Infinity), 0);
assert.equal(b.indexOf(buf_a, Infinity), -1);
assert.equal(b.indexOf(buf_bc), 1);
assert.equal(b.indexOf(buf_bc, 2), -1);
assert.equal(b.indexOf(buf_bc, -1), -1);
assert.equal(b.indexOf(buf_bc, -3), -1);
assert.equal(b.indexOf(buf_bc, -5), 1);
assert.equal(b.indexOf(buf_bc, NaN), 1);
assert.equal(b.indexOf(buf_bc, -Infinity), 1);
assert.equal(b.indexOf(buf_bc, Infinity), -1);
assert.equal(b.indexOf(buf_f), b.length - 1);
assert.equal(b.indexOf(buf_z), -1);
assert.equal(b.indexOf(buf_empty), -1);
assert.equal(b.indexOf(buf_empty, 1), -1);
assert.equal(b.indexOf(buf_empty, b.length + 1), -1);
assert.equal(b.indexOf(buf_empty, Infinity), -1);
assert.equal(b.indexOf(0x61), 0);
assert.equal(b.indexOf(0x61, 1), -1);
assert.equal(b.indexOf(0x61, -1), -1);
assert.equal(b.indexOf(0x61, -4), -1);
assert.equal(b.indexOf(0x61, -b.length), 0);
assert.equal(b.indexOf(0x61, NaN), 0);
assert.equal(b.indexOf(0x61, -Infinity), 0);
assert.equal(b.indexOf(0x61, Infinity), -1);
assert.equal(b.indexOf(0x0), -1);

assert.throws(function() {
  b.indexOf(function() { });
});
assert.throws(function() {
  b.indexOf({});
});
assert.throws(function() {
  b.indexOf([]);
});
