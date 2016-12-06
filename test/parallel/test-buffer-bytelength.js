'use strict';

require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;
const SlowBuffer = require('buffer').SlowBuffer;
const vm = require('vm');

// coerce values to string
assert.throws(() => { Buffer.byteLength(32, 'latin1'); },
              /"string" must be a string, Buffer, or ArrayBuffer/);
assert.throws(() => { Buffer.byteLength(NaN, 'utf8'); },
              /"string" must be a string, Buffer, or ArrayBuffer/);
assert.throws(() => { Buffer.byteLength({}, 'latin1'); },
              /"string" must be a string, Buffer, or ArrayBuffer/);
assert.throws(() => { Buffer.byteLength(); },
              /"string" must be a string, Buffer, or ArrayBuffer/);

assert(ArrayBuffer.isView(new Buffer(10)));
assert(ArrayBuffer.isView(new SlowBuffer(10)));
assert(ArrayBuffer.isView(Buffer.alloc(10)));
assert(ArrayBuffer.isView(Buffer.allocUnsafe(10)));
assert(ArrayBuffer.isView(Buffer.allocUnsafeSlow(10)));
assert(ArrayBuffer.isView(Buffer.from('')));

// buffer
var incomplete = Buffer.from([0xe4, 0xb8, 0xad, 0xe6, 0x96]);
assert.strictEqual(Buffer.byteLength(incomplete), 5);
var ascii = Buffer.from('abc');
assert.strictEqual(Buffer.byteLength(ascii), 3);

// ArrayBuffer
var buffer = new ArrayBuffer(8);
assert.strictEqual(Buffer.byteLength(buffer), 8);

// TypedArray
var int8 = new Int8Array(8);
assert.strictEqual(Buffer.byteLength(int8), 8);
var uint8 = new Uint8Array(8);
assert.strictEqual(Buffer.byteLength(uint8), 8);
var uintc8 = new Uint8ClampedArray(2);
assert.strictEqual(Buffer.byteLength(uintc8), 2);
var int16 = new Int16Array(8);
assert.strictEqual(Buffer.byteLength(int16), 16);
var uint16 = new Uint16Array(8);
assert.strictEqual(Buffer.byteLength(uint16), 16);
var int32 = new Int32Array(8);
assert.strictEqual(Buffer.byteLength(int32), 32);
var uint32 = new Uint32Array(8);
assert.strictEqual(Buffer.byteLength(uint32), 32);
var float32 = new Float32Array(8);
assert.strictEqual(Buffer.byteLength(float32), 32);
var float64 = new Float64Array(8);
assert.strictEqual(Buffer.byteLength(float64), 64);

// DataView
var dv = new DataView(new ArrayBuffer(2));
assert.strictEqual(Buffer.byteLength(dv), 2);

// special case: zero length string
assert.strictEqual(Buffer.byteLength('', 'ascii'), 0);
assert.strictEqual(Buffer.byteLength('', 'HeX'), 0);

// utf8
assert.strictEqual(Buffer.byteLength('∑éllö wørl∂!', 'utf-8'), 19);
assert.strictEqual(Buffer.byteLength('κλμνξο', 'utf8'), 12);
assert.strictEqual(Buffer.byteLength('挵挶挷挸挹', 'utf-8'), 15);
assert.strictEqual(Buffer.byteLength('𠝹𠱓𠱸', 'UTF8'), 12);
// without an encoding, utf8 should be assumed
assert.strictEqual(Buffer.byteLength('hey there'), 9);
assert.strictEqual(Buffer.byteLength('𠱸挶νξ#xx :)'), 17);
assert.strictEqual(Buffer.byteLength('hello world', ''), 11);
// it should also be assumed with unrecognized encoding
assert.strictEqual(Buffer.byteLength('hello world', 'abc'), 11);
assert.strictEqual(Buffer.byteLength('ßœ∑≈', 'unkn0wn enc0ding'), 10);

// base64
assert.strictEqual(Buffer.byteLength('aGVsbG8gd29ybGQ=', 'base64'), 11);
assert.strictEqual(Buffer.byteLength('bm9kZS5qcyByb2NrcyE=', 'base64'), 14);
assert.strictEqual(Buffer.byteLength('aGkk', 'base64'), 3);
assert.strictEqual(
  Buffer.byteLength('bHNrZGZsa3NqZmtsc2xrZmFqc2RsZmtqcw==', 'base64'), 25
);
// special padding
assert.strictEqual(Buffer.byteLength('aaa=', 'base64'), 2);
assert.strictEqual(Buffer.byteLength('aaaa==', 'base64'), 3);

assert.strictEqual(Buffer.byteLength('Il était tué'), 14);
assert.strictEqual(Buffer.byteLength('Il était tué', 'utf8'), 14);
assert.strictEqual(Buffer.byteLength('Il était tué', 'ascii'), 12);
assert.strictEqual(Buffer.byteLength('Il était tué', 'latin1'), 12);
assert.strictEqual(Buffer.byteLength('Il était tué', 'binary'), 12);
['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach(function(encoding) {
  assert.strictEqual(24, Buffer.byteLength('Il était tué', encoding));
});

// Test that ArrayBuffer from a different context is detected correctly
const arrayBuf = vm.runInNewContext('new ArrayBuffer()');
assert.strictEqual(Buffer.byteLength(arrayBuf), 0);
