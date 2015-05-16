'use strict';

var common = require('../common');
var assert = require('assert');
var Buffer = require('buffer').Buffer;

// coerce values to string
assert.equal(Buffer.byteLength(32, 'raw'), 2);
assert.equal(Buffer.byteLength(NaN, 'utf8'), 3);
assert.equal(Buffer.byteLength({}, 'raws'), 15);
assert.equal(Buffer.byteLength(), 9);

// special case: zero length string
assert.equal(Buffer.byteLength('', 'ascii'), 0);
assert.equal(Buffer.byteLength('', 'HeX'), 0);

// utf8
assert.equal(Buffer.byteLength('∑éllö wørl∂!', 'utf-8'), 19);
assert.equal(Buffer.byteLength('κλμνξο', 'utf8'), 12);
assert.equal(Buffer.byteLength('挵挶挷挸挹', 'utf-8'), 15);
assert.equal(Buffer.byteLength('𠝹𠱓𠱸', 'UTF8'), 12);
// without an encoding, utf8 should be assumed
assert.equal(Buffer.byteLength('hey there'), 9);
assert.equal(Buffer.byteLength('𠱸挶νξ#xx :)'), 17);
assert.equal(Buffer.byteLength('hello world', ''), 11);
// it should also be assumed with unrecognized encoding
assert.equal(Buffer.byteLength('hello world', 'abc'), 11);
assert.equal(Buffer.byteLength('ßœ∑≈', 'unkn0wn enc0ding'), 10);

// base64
assert.equal(Buffer.byteLength('aGVsbG8gd29ybGQ=', 'base64'), 11);
assert.equal(Buffer.byteLength('bm9kZS5qcyByb2NrcyE=', 'base64'), 14);
assert.equal(Buffer.byteLength('aGkk', 'base64'), 3);
assert.equal(Buffer.byteLength('bHNrZGZsa3NqZmtsc2xrZmFqc2RsZmtqcw==',
    'base64'), 25);
// special padding
assert.equal(Buffer.byteLength('aaa=', 'base64'), 2);
assert.equal(Buffer.byteLength('aaaa==', 'base64'), 3);

assert.equal(Buffer.byteLength('Il était tué'), 14);
assert.equal(Buffer.byteLength('Il était tué', 'utf8'), 14);
assert.equal(Buffer.byteLength('Il était tué', 'ascii'), 12);
assert.equal(Buffer.byteLength('Il était tué', 'binary'), 12);
['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach(function(encoding) {
  assert.equal(24, Buffer.byteLength('Il était tué', encoding));
});
