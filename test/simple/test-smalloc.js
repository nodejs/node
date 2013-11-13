// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

// first grab js api's
var smalloc = require('smalloc');
var alloc = smalloc.alloc;
var dispose = smalloc.dispose;
var copyOnto = smalloc.copyOnto;
var kMaxLength = smalloc.kMaxLength;
var Types = smalloc.Types;
// sliceOnto is volatile and cannot be exposed to users.
var sliceOnto = process.binding('smalloc').sliceOnto;


// verify allocation

var b = alloc(5, {});
assert.ok(typeof b === 'object');
for (var i = 0; i < 5; i++)
  assert.ok(b[i] !== undefined);


var b = {};
var c = alloc(5, b);
assert.equal(b, c);
assert.deepEqual(b, c);


var b = alloc(5, {});
var c = {};
c._data = sliceOnto(b, c, 0, 5);
assert.ok(typeof c._data === 'object');
assert.equal(b, c._data);
assert.deepEqual(b, c._data);


// verify writes

var b = alloc(5, {});
for (var i = 0; i < 5; i++)
  b[i] = i;
for (var i = 0; i < 5; i++)
  assert.equal(b[i], i);


var b = alloc(1, Types.Uint8);
b[0] = 256;
assert.equal(b[0], 0);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Int8);
b[0] = 128;
assert.equal(b[0], -128);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Uint16);
b[0] = 65536;
assert.equal(b[0], 0);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Int16);
b[0] = 32768;
assert.equal(b[0], -32768);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Uint32);
b[0] = 4294967296;
assert.equal(b[0], 0);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Int32);
b[0] = 2147483648;
assert.equal(b[0], -2147483648);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Float);
b[0] = 0.1111111111111111;
assert.equal(b[0], 0.1111111119389534);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Double);
b[0] = 0.1111111111111111;
assert.equal(b[0], 0.1111111111111111);
assert.equal(b[1], undefined);


var b = alloc(1, Types.Uint8Clamped);
b[0] = 300;
assert.equal(b[0], 255);
assert.equal(b[1], undefined);


var b = alloc(6, {});
var c0 = {};
var c1 = {};
c0._data = sliceOnto(b, c0, 0, 3);
c1._data = sliceOnto(b, c1, 3, 6);
for (var i = 0; i < 3; i++) {
  c0[i] = i;
  c1[i] = i + 3;
}
for (var i = 0; i < 3; i++)
  assert.equal(b[i], i);
for (var i = 3; i < 6; i++)
  assert.equal(b[i], i);


var a = alloc(6, {});
var b = alloc(6, {});
var c = alloc(12, {});
for (var i = 0; i < 6; i++) {
  a[i] = i;
  b[i] = i * 2;
}
copyOnto(a, 0, c, 0, 6);
copyOnto(b, 0, c, 6, 6);
for (var i = 0; i < 6; i++) {
  assert.equal(c[i], i);
  assert.equal(c[i + 6], i * 2);
}


var b = alloc(1, Types.Double);
var c = alloc(2, Types.Uint32);
c[0] = 2576980378;
c[1] = 1069128089;
copyOnto(c, 0, b, 0, 2);
assert.equal(b[0], 0.1);


// verify checking external if has external memory

// check objects
var b = {};
assert.ok(!smalloc.hasExternalData(b));
alloc(1, b);
assert.ok(smalloc.hasExternalData(b));
var f = function() { };
alloc(1, f);
assert.ok(smalloc.hasExternalData(f));

// and non-objects
assert.ok(!smalloc.hasExternalData(true));
assert.ok(!smalloc.hasExternalData(1));
assert.ok(!smalloc.hasExternalData('string'));
assert.ok(!smalloc.hasExternalData(null));
assert.ok(!smalloc.hasExternalData());


// verify alloc throws properly

// arrays are not supported
assert.throws(function() {
  alloc(0, []);
}, TypeError);


// no allocations larger than kMaxLength
assert.throws(function() {
  alloc(kMaxLength + 1);
}, RangeError);


// properly convert to uint32 before checking overflow
assert.throws(function() {
  alloc(-1);
}, RangeError);


// no allocating on what's been allocated
assert.throws(function() {
  alloc(1, alloc(1));
}, TypeError);


// throw for values passed that are not objects
assert.throws(function() {
  alloc(1, 'a');
}, TypeError);
assert.throws(function() {
  alloc(1, true);
}, TypeError);
assert.throws(function() {
  alloc(1, null);
}, TypeError);


// should not throw allocating to most objects
alloc(1, function() { });
alloc(1, /abc/);
alloc(1, new Date());


// range check on external array enumeration
assert.throws(function() {
  alloc(1, 0);
}, TypeError);
assert.throws(function() {
  alloc(1, 10);
}, TypeError);

// very copyOnto throws properly

// source must have data
assert.throws(function() {
  copyOnto({}, 0, alloc(1), 0, 0);
}, Error);


// dest must have data
assert.throws(function() {
  copyOnto(alloc(1), 0, {}, 0, 0);
}, Error);


// copyLength <= sourceLength
assert.throws(function() {
  copyOnto(alloc(1), 0, alloc(3), 0, 2);
}, RangeError);


// copyLength <= destLength
assert.throws(function() {
  copyOnto(alloc(3), 0, alloc(1), 0, 2);
}, RangeError);


// sourceStart <= sourceLength
assert.throws(function() {
  copyOnto(alloc(1), 3, alloc(1), 0, 1);
}, RangeError);


// destStart <= destLength
assert.throws(function() {
  copyOnto(alloc(1), 0, alloc(1), 3, 1);
}, RangeError);


// sourceStart + copyLength <= sourceLength
assert.throws(function() {
  copyOnto(alloc(3), 1, alloc(3), 0, 3);
}, RangeError);


// destStart + copyLength <= destLength
assert.throws(function() {
  copyOnto(alloc(3), 0, alloc(3), 1, 3);
}, RangeError);


// copy_length * array_size <= dest_length
assert.throws(function() {
  copyOnto(alloc(2, Types.Double), 0, alloc(2, Types.Uint32), 0, 2);
}, RangeError);


// test disposal
var b = alloc(5, {});
dispose(b);
for (var i = 0; i < 5; i++)
  assert.equal(b[i], undefined);


// verify dispose throws properly

// only allow object to be passed to dispose
assert.throws(function() {
  alloc.dispose(null);
});


// can't dispose a Buffer
assert.throws(function() {
  alloc.dispose(new Buffer());
});
