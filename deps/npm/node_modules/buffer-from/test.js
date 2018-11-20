var bufferFrom = require('./')
var assert = require('assert')

assert.equal(bufferFrom([1, 2, 3, 4]).toString('hex'), '01020304')

var arr = new Uint8Array([1, 2, 3, 4])
assert.equal(bufferFrom(arr.buffer, 1, 2).toString('hex'), '0203')

assert.equal(bufferFrom('test', 'utf8').toString('hex'), '74657374')

var buf = bufferFrom('test')
assert.equal(bufferFrom(buf).toString('hex'), '74657374')
