// Copyright 2015 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --harmony-sharedarraybuffer --allow-natives-syntax

// SharedArrayBuffer

function TestByteLength(param, expectedByteLength) {
  var sab = new SharedArrayBuffer(param);
  assertSame(expectedByteLength, sab.byteLength);
}

function TestArrayBufferCreation() {
  TestByteLength(1, 1);
  TestByteLength(256, 256);
  TestByteLength(2.567, 2);

  TestByteLength("abc", 0);

  TestByteLength(0, 0);

  assertThrows(function() { new SharedArrayBuffer(-10); }, RangeError);
  assertThrows(function() { new SharedArrayBuffer(-2.567); }, RangeError);

  assertThrows(function() {
    let kArrayBufferByteLengthLimit = %ArrayBufferMaxByteLength() + 1;
    var ab1 = new SharedArrayBuffer(kArrayBufferByteLengthLimit);
  }, RangeError);

  var sab = new SharedArrayBuffer();
  assertSame(0, sab.byteLength);
  assertEquals("[object SharedArrayBuffer]",
      Object.prototype.toString.call(sab));
}

TestArrayBufferCreation();

function TestByteLengthNotWritable() {
  var sab = new SharedArrayBuffer(1024);
  assertSame(1024, sab.byteLength);

  assertThrows(function() { "use strict"; sab.byteLength = 42; }, TypeError);
}

TestByteLengthNotWritable();

// Typed arrays using SharedArrayBuffers

// TODO(binji): how many of these tests are necessary if there are no new
// TypedArray types?

function MakeSharedTypedArray(constr, numElements) {
  var sab = new SharedArrayBuffer(constr.BYTES_PER_ELEMENT * numElements);
  return new constr(sab);
}

function TestTypedArray(constr, elementSize, typicalElement) {
  assertSame(elementSize, constr.BYTES_PER_ELEMENT);

  var sab = new SharedArrayBuffer(256*elementSize);

  var a0 = new constr(30);
  assertEquals("[object " + constr.name + "]",
      Object.prototype.toString.call(a0));

  assertSame(elementSize, a0.BYTES_PER_ELEMENT);
  assertSame(30, a0.length);
  assertSame(30*elementSize, a0.byteLength);
  assertSame(0, a0.byteOffset);
  assertSame(30*elementSize, a0.buffer.byteLength);

  var aOverBufferLen0 = new constr(sab, 128*elementSize, 0);
  assertSame(sab, aOverBufferLen0.buffer);
  assertSame(elementSize, aOverBufferLen0.BYTES_PER_ELEMENT);
  assertSame(0, aOverBufferLen0.length);
  assertSame(0, aOverBufferLen0.byteLength);
  assertSame(128*elementSize, aOverBufferLen0.byteOffset);

  var a1 = new constr(sab, 128*elementSize, 128);
  assertSame(sab, a1.buffer);
  assertSame(elementSize, a1.BYTES_PER_ELEMENT);
  assertSame(128, a1.length);
  assertSame(128*elementSize, a1.byteLength);
  assertSame(128*elementSize, a1.byteOffset);


  var a2 = new constr(sab, 64*elementSize, 128);
  assertSame(sab, a2.buffer);
  assertSame(elementSize, a2.BYTES_PER_ELEMENT);
  assertSame(128, a2.length);
  assertSame(128*elementSize, a2.byteLength);
  assertSame(64*elementSize, a2.byteOffset);

  var a3 = new constr(sab, 192*elementSize);
  assertSame(sab, a3.buffer);
  assertSame(64, a3.length);
  assertSame(64*elementSize, a3.byteLength);
  assertSame(192*elementSize, a3.byteOffset);

  var a4 = new constr(sab);
  assertSame(sab, a4.buffer);
  assertSame(256, a4.length);
  assertSame(256*elementSize, a4.byteLength);
  assertSame(0, a4.byteOffset);


  var i;
  for (i = 0; i < 128; i++) {
    a1[i] = typicalElement;
  }

  for (i = 0; i < 128; i++) {
    assertSame(typicalElement, a1[i]);
  }

  for (i = 0; i < 64; i++) {
    assertSame(0, a2[i]);
  }

  for (i = 64; i < 128; i++) {
    assertSame(typicalElement, a2[i]);
  }

  for (i = 0; i < 64; i++) {
    assertSame(typicalElement, a3[i]);
  }

  for (i = 0; i < 128; i++) {
    assertSame(0, a4[i]);
  }

  for (i = 128; i < 256; i++) {
    assertSame(typicalElement, a4[i]);
  }

  var aAtTheEnd = new constr(sab, 256*elementSize);
  assertSame(elementSize, aAtTheEnd.BYTES_PER_ELEMENT);
  assertSame(0, aAtTheEnd.length);
  assertSame(0, aAtTheEnd.byteLength);
  assertSame(256*elementSize, aAtTheEnd.byteOffset);

  assertThrows(function () { new constr(sab, 257*elementSize); }, RangeError);
  assertThrows(
      function () { new constr(sab, 128*elementSize, 192); },
      RangeError);

  if (elementSize !== 1) {
    assertThrows(function() { new constr(sab, 128*elementSize - 1, 10); },
                 RangeError);
    var unalignedArrayBuffer = new SharedArrayBuffer(10*elementSize + 1);
    var goodArray = new constr(unalignedArrayBuffer, 0, 10);
    assertSame(10, goodArray.length);
    assertSame(10*elementSize, goodArray.byteLength);
    assertThrows(function() { new constr(unalignedArrayBuffer)}, RangeError);
    assertThrows(function() { new constr(unalignedArrayBuffer, 5*elementSize)},
                 RangeError);
  }

  var abLen0 = new SharedArrayBuffer(0);
  var aOverAbLen0 = new constr(abLen0);
  assertSame(abLen0, aOverAbLen0.buffer);
  assertSame(elementSize, aOverAbLen0.BYTES_PER_ELEMENT);
  assertSame(0, aOverAbLen0.length);
  assertSame(0, aOverAbLen0.byteLength);
  assertSame(0, aOverAbLen0.byteOffset);

  var a = new constr(sab, 64*elementSize, 128);
  assertEquals("[object " + constr.name + "]",
      Object.prototype.toString.call(a));
  var desc = Object.getOwnPropertyDescriptor(
      constr.prototype.__proto__, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.enumerable);
  assertFalse(!!desc.writable);
  assertFalse(!!desc.set);
  assertEquals("function", typeof desc.get);
}

TestTypedArray(Uint8Array, 1, 0xFF);
TestTypedArray(Int8Array, 1, -0x7F);
TestTypedArray(Uint16Array, 2, 0xFFFF);
TestTypedArray(Int16Array, 2, -0x7FFF);
TestTypedArray(Uint32Array, 4, 0xFFFFFFFF);
TestTypedArray(Int32Array, 4, -0x7FFFFFFF);
TestTypedArray(Float32Array, 4, 0.5);
TestTypedArray(Float64Array, 8, 0.5);
TestTypedArray(Uint8ClampedArray, 1, 0xFF);


function SubarrayTestCase(constructor, item, expectedResultLen,
                          expectedStartIndex, initialLen, start, end) {
  var a = MakeSharedTypedArray(constructor, initialLen);
  var s = a.subarray(start, end);
  assertSame(constructor, s.constructor);
  assertSame(expectedResultLen, s.length);
  if (s.length > 0) {
    s[0] = item;
    assertSame(item, a[expectedStartIndex]);
  }
}

function TestSubArray(constructor, item) {
  SubarrayTestCase(constructor, item, 512, 512, 1024, 512, 1024);
  SubarrayTestCase(constructor, item, 512, 512, 1024, 512);

  SubarrayTestCase(constructor, item, 0, undefined, 0, 1, 20);
  SubarrayTestCase(constructor, item, 100, 0,       100, 0, 100);
  SubarrayTestCase(constructor, item, 100, 0,       100,  0, 1000);
  SubarrayTestCase(constructor, item, 0, undefined, 100, 5, 1);

  SubarrayTestCase(constructor, item, 1, 89,        100, -11, -10);
  SubarrayTestCase(constructor, item, 9, 90,        100, -10, 99);
  SubarrayTestCase(constructor, item, 0, undefined, 100, -10, 80);
  SubarrayTestCase(constructor, item, 10,80,        100, 80, -10);

  SubarrayTestCase(constructor, item, 10,90,        100, 90, "100");
  SubarrayTestCase(constructor, item, 10,90,        100, "90", "100");

  SubarrayTestCase(constructor, item, 0, undefined, 100, 90, "abc");
  SubarrayTestCase(constructor, item, 10,0,         100, "abc", 10);

  SubarrayTestCase(constructor, item, 10,0,         100, 0.96, 10.96);
  SubarrayTestCase(constructor, item, 10,0,         100, 0.96, 10.01);
  SubarrayTestCase(constructor, item, 10,0,         100, 0.01, 10.01);
  SubarrayTestCase(constructor, item, 10,0,         100, 0.01, 10.96);


  SubarrayTestCase(constructor, item, 10,90,        100, 90);
  SubarrayTestCase(constructor, item, 10,90,        100, -10);
}

TestSubArray(Uint8Array, 0xFF);
TestSubArray(Int8Array, -0x7F);
TestSubArray(Uint16Array, 0xFFFF);
TestSubArray(Int16Array, -0x7FFF);
TestSubArray(Uint32Array, 0xFFFFFFFF);
TestSubArray(Int32Array, -0x7FFFFFFF);
TestSubArray(Float32Array, 0.5);
TestSubArray(Float64Array, 0.5);
TestSubArray(Uint8ClampedArray, 0xFF);

function TestTypedArrayOutOfRange(constructor, value, result) {
  var a = MakeSharedTypedArray(constructor, 1);
  a[0] = value;
  assertSame(result, a[0]);
}

TestTypedArrayOutOfRange(Uint8Array, 0x1FA, 0xFA);
TestTypedArrayOutOfRange(Uint8Array, -1, 0xFF);

TestTypedArrayOutOfRange(Int8Array, 0x1FA, 0x7A - 0x80);

TestTypedArrayOutOfRange(Uint16Array, 0x1FFFA, 0xFFFA);
TestTypedArrayOutOfRange(Uint16Array, -1, 0xFFFF);
TestTypedArrayOutOfRange(Int16Array, 0x1FFFA, 0x7FFA - 0x8000);

TestTypedArrayOutOfRange(Uint32Array, 0x1FFFFFFFA, 0xFFFFFFFA);
TestTypedArrayOutOfRange(Uint32Array, -1, 0xFFFFFFFF);
TestTypedArrayOutOfRange(Int32Array, 0x1FFFFFFFA, 0x7FFFFFFA - 0x80000000);

TestTypedArrayOutOfRange(Uint8ClampedArray, 0x1FA, 0xFF);
TestTypedArrayOutOfRange(Uint8ClampedArray, -1, 0);

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array];

function TestPropertyTypeChecks(constructor) {
  function CheckProperty(name) {
    var d = Object.getOwnPropertyDescriptor(constructor.prototype.__proto__,
                                            name);
    var o = {};
    assertThrows(function() {d.get.call(o);}, TypeError);
    for (var i = 0; i < typedArrayConstructors.length; i++) {
      var ctor = typedArrayConstructors[i];
      var a = MakeSharedTypedArray(ctor, 10);
      d.get.call(a); // shouldn't throw
    }
  }

  CheckProperty("buffer");
  CheckProperty("byteOffset");
  CheckProperty("byteLength");
  CheckProperty("length");
}

for(i = 0; i < typedArrayConstructors.length; i++) {
  TestPropertyTypeChecks(typedArrayConstructors[i]);
}

function TestTypedArraySet() {
  // Test array.set in different combinations.

  function assertArrayPrefix(expected, array) {
    for (var i = 0; i < expected.length; ++i) {
      assertEquals(expected[i], array[i]);
    }
  }

  // SharedTypedArrays don't allow initialization via array-like
  function initializeFromArray(constructor, array) {
    var buffer = MakeSharedTypedArray(constructor, array.length);
    for (var i = 0; i < array.length; ++i) {
      buffer[i] = array[i];
    }
    return buffer;
  }

  var a11 = initializeFromArray(Int16Array, [1, 2, 3, 4, 0, -1])
  var a12 = MakeSharedTypedArray(Uint16Array, 15);
  a12.set(a11, 3)
  assertArrayPrefix([0, 0, 0, 1, 2, 3, 4, 0, 0xffff, 0, 0], a12)
  assertThrows(function(){ a11.set(a12) })

  var a21 = [1, undefined, 10, NaN, 0, -1, {valueOf: function() {return 3}}]
  var a22 = MakeSharedTypedArray(Int32Array, 12)
  a22.set(a21, 2)
  assertArrayPrefix([0, 0, 1, 0, 10, 0, 0, -1, 3, 0], a22)

  var a31 = initializeFromArray(Float32Array, [2, 4, 6, 8, 11, NaN, 1/0, -3])
  var a32 = a31.subarray(2, 6)
  a31.set(a32, 4)
  assertArrayPrefix([2, 4, 6, 8, 6, 8, 11, NaN], a31)
  assertArrayPrefix([6, 8, 6, 8], a32)

  var a4 = initializeFromArray(Uint8ClampedArray, [3,2,5,6])
  a4.set(a4)
  assertArrayPrefix([3, 2, 5, 6], a4)

  // Cases with overlapping backing store but different element sizes.
  var b = new SharedArrayBuffer(4)
  var a5 = new Int16Array(b)
  var a50 = new Int8Array(b)
  var a51 = new Int8Array(b, 0, 2)
  var a52 = new Int8Array(b, 1, 2)
  var a53 = new Int8Array(b, 2, 2)

  a5.set([0x5050, 0x0a0a])
  assertArrayPrefix([0x50, 0x50, 0x0a, 0x0a], a50)
  assertArrayPrefix([0x50, 0x50], a51)
  assertArrayPrefix([0x50, 0x0a], a52)
  assertArrayPrefix([0x0a, 0x0a], a53)

  a50.set([0x50, 0x50, 0x0a, 0x0a])
  a51.set(a5)
  assertArrayPrefix([0x50, 0x0a, 0x0a, 0x0a], a50)

  a50.set([0x50, 0x50, 0x0a, 0x0a])
  a52.set(a5)
  assertArrayPrefix([0x50, 0x50, 0x0a, 0x0a], a50)

  a50.set([0x50, 0x50, 0x0a, 0x0a])
  a53.set(a5)
  assertArrayPrefix([0x50, 0x50, 0x50, 0x0a], a50)

  a50.set([0x50, 0x51, 0x0a, 0x0b])
  a5.set(a51)
  assertArrayPrefix([0x0050, 0x0051], a5)

  a50.set([0x50, 0x51, 0x0a, 0x0b])
  a5.set(a52)
  assertArrayPrefix([0x0051, 0x000a], a5)

  a50.set([0x50, 0x51, 0x0a, 0x0b])
  a5.set(a53)
  assertArrayPrefix([0x000a, 0x000b], a5)

  // Mixed types of same size.
  var a61 = initializeFromArray(Float32Array, [1.2, 12.3])
  var a62 = MakeSharedTypedArray(Int32Array, 2)
  a62.set(a61)
  assertArrayPrefix([1, 12], a62)
  a61.set(a62)
  assertArrayPrefix([1, 12], a61)

  // Invalid source
  var a = MakeSharedTypedArray(Uint16Array, 50);
  var expected = [];
  for (i = 0; i < 50; i++) {
    a[i] = i;
    expected.push(i);
  }
  a.set({});
  assertArrayPrefix(expected, a);
  assertThrows(function() { a.set.call({}) }, TypeError);
  assertThrows(function() { a.set.call([]) }, TypeError);

  assertThrows(function() { a.set(0); }, TypeError);
  assertThrows(function() { a.set(0, 1); }, TypeError);
}

TestTypedArraySet();

function TestTypedArraysWithIllegalIndices() {
  var a = MakeSharedTypedArray(Int32Array, 100);

  a[-10] = 10;
  assertEquals(undefined, a[-10]);
  a["-10"] = 10;
  assertEquals(undefined, a["-10"]);

  var s = "    -10";
  a[s] = 10;
  assertEquals(10, a[s]);
  var s1 = "    -10   ";
  a[s] = 10;
  assertEquals(10, a[s]);

  a["-1e2"] = 10;
  assertEquals(10, a["-1e2"]);
  assertEquals(undefined, a[-1e2]);

  a["-0"] = 256;
  var s2 = "     -0";
  a[s2] = 255;
  assertEquals(undefined, a["-0"]);
  assertEquals(255, a[s2]);
  assertEquals(0, a[-0]);


  a[-Infinity] = 50;
  assertEquals(undefined, a[-Infinity]);

  a[1.5] = 10;
  assertEquals(undefined, a[1.5]);
  var nan = Math.sqrt(-1);
  a[nan] = 5;
  assertEquals(undefined, a[nan]);

  var x = 0;
  var y = -0;
  assertEquals(Infinity, 1/x);
  assertEquals(-Infinity, 1/y);
  a[x] = 5;
  a[y] = 27;
  assertEquals(27, a[x]);
  assertEquals(27, a[y]);
}

TestTypedArraysWithIllegalIndices();

function TestTypedArraysWithIllegalIndicesStrict() {
  'use strict';
  var a = MakeSharedTypedArray(Int32Array, 100);

  a[-10] = 10;
  assertEquals(undefined, a[-10]);
  a["-10"] = 10;
  assertEquals(undefined, a["-10"]);

  var s = "    -10";
  a[s] = 10;
  assertEquals(10, a[s]);
  var s1 = "    -10   ";
  a[s] = 10;
  assertEquals(10, a[s]);

  a["-1e2"] = 10;
  assertEquals(10, a["-1e2"]);
  assertEquals(undefined, a[-1e2]);

  a["-0"] = 256;
  var s2 = "     -0";
  a[s2] = 255;
  assertEquals(undefined, a["-0"]);
  assertEquals(255, a[s2]);
  assertEquals(0, a[-0]);

  a[-Infinity] = 50;
  assertEquals(undefined, a[-Infinity]);

  a[1.5] = 10;
  assertEquals(undefined, a[1.5]);
  var nan = Math.sqrt(-1);
  a[nan] = 5;
  assertEquals(undefined, a[nan]);

  var x = 0;
  var y = -0;
  assertEquals(Infinity, 1/x);
  assertEquals(-Infinity, 1/y);
  a[x] = 5;
  a[y] = 27;
  assertEquals(27, a[x]);
  assertEquals(27, a[y]);
}

TestTypedArraysWithIllegalIndicesStrict();

// General tests for properties

// Test property attribute [[Enumerable]]
function TestEnumerable(func, obj) {
  function props(x) {
    var array = [];
    for (var p in x) array.push(p);
    return array.sort();
  }
  assertArrayEquals([], props(func));
  assertArrayEquals([], props(func.prototype));
  if (obj)
    assertArrayEquals([], props(obj));
}
TestEnumerable(ArrayBuffer, new SharedArrayBuffer());
for(i = 0; i < typedArrayConstructors.length; i++) {
  TestEnumerable(typedArrayConstructors[i]);
}

// Test arbitrary properties on ArrayBuffer
function TestArbitrary(m) {
  function TestProperty(map, property, value) {
    map[property] = value;
    assertEquals(value, map[property]);
  }
  for (var i = 0; i < 20; i++) {
    TestProperty(m, 'key' + i, 'val' + i);
    TestProperty(m, 'foo' + i, 'bar' + i);
  }
}
TestArbitrary(new SharedArrayBuffer(256));
for(i = 0; i < typedArrayConstructors.length; i++) {
  TestArbitrary(MakeSharedTypedArray(typedArrayConstructors[i], 10));
}

// Test direct constructor call
assertThrows(function() { SharedArrayBuffer(); }, TypeError);
for(i = 0; i < typedArrayConstructors.length; i++) {
  assertThrows(function(i) { typedArrayConstructors[i](); }.bind(this, i),
               TypeError);
}

// byteLength from prototype can be overwritten
var s = new SharedArrayBuffer(10);
assertEquals(10, s.byteLength);
Object.defineProperty(s, 'byteLength', {value: 42});
assertEquals(42, s.byteLength);

// byteLength on incompatible type (shared vs. regular ArrayBuffer)
var desc = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength');
s = new SharedArrayBuffer(10);
Object.defineProperty(s, 'byteLength', desc);
assertThrows(function() {s.byteLength}, TypeError);

desc = Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype,
  'byteLength');
var a = new ArrayBuffer(10);
Object.defineProperty(a, 'byteLength', desc);
assertThrows(function() {a.byteLength}, TypeError);

// test SharedArrayBuffer species getter
assertSame(SharedArrayBuffer[Symbol.species], SharedArrayBuffer);
var desc = Object.getOwnPropertyDescriptor(SharedArrayBuffer, Symbol.species);
assertEquals("function", typeof desc.get);
assertEquals("get [Symbol.species]", desc.get.name);
assertEquals(0, desc.get.length);
assertEquals("undefined", typeof desc.set);
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
