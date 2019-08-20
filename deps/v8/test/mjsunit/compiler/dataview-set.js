// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

var buffer = new ArrayBuffer(64);
var dataview = new DataView(buffer, 8, 24);

function writeUint8(offset, value) {
  dataview.setUint8(offset, value);
}

function writeInt8Handled(offset, value) {
  try {
    dataview.setInt8(offset, value);
  } catch(e) {
    return e;
  }
}

function writeUint16(offset, value, little_endian) {
  dataview.setUint16(offset, value, little_endian);
}

function writeInt16(offset, value, little_endian) {
  dataview.setInt16(offset, value, little_endian);
}

function writeUint32(offset, value, little_endian) {
  dataview.setUint32(offset, value, little_endian);
}

function writeInt32(offset, value, little_endian) {
  dataview.setInt32(offset, value, little_endian);
}

function writeFloat32(offset, value, little_endian) {
  dataview.setFloat32(offset, value, little_endian);
}

function writeFloat64(offset, value, little_endian) {
  dataview.setFloat64(offset, value, little_endian);
}

function warmup(f) {
  %PrepareFunctionForOptimization(f);
  f(0, 0);
  f(0, 1);
  %OptimizeFunctionOnNextCall(f);
  f(0, 2);
  f(0, 3);
}

// TurboFan valid setUint8.
warmup(writeUint8);
assertOptimized(writeUint8);
writeUint8(0, 0xde);
writeUint8(1, 0xad);
writeUint8(2, 0xbe);
writeUint8(3, 0xef);
assertEquals(0xdeadbeef, dataview.getUint32(0));

// TurboFan valid setInt8.
warmup(writeInt8Handled);
assertOptimized(writeInt8Handled);
writeInt8Handled(0, -34);
writeInt8Handled(1, -83);
writeInt8Handled(2, -66);
writeInt8Handled(3, -17);
assertEquals(0xdeadbeef, dataview.getUint32(0));

// TurboFan valid setUint16.
warmup(writeUint16);
assertOptimized(writeUint16);
writeUint16(0, 0xdead);
writeUint16(2, 0xefbe, true);
assertEquals(0xdeadbeef, dataview.getUint32(0));

// TurboFan valid setInt16.
warmup(writeInt16);
assertOptimized(writeInt16);
writeInt16(0, -8531);
writeInt16(2, -4162, true);
assertEquals(0xdeadbeef, dataview.getUint32(0));

// TurboFan valid setUint32.
warmup(writeUint32);
assertOptimized(writeUint32);
writeUint32(0, 0xdeadbeef);
assertEquals(0xdeadbeef, dataview.getUint32(0));
writeUint32(0, 0xefbeadde, true);
assertEquals(0xdeadbeef, dataview.getUint32(0));

// TurboFan valid setInt32.
warmup(writeInt32);
assertOptimized(writeInt32);
writeInt32(0, -559038737);
assertEquals(0xdeadbeef, dataview.getUint32(0));
writeInt32(0, -272716322, true);
assertEquals(0xdeadbeef, dataview.getUint32(0));

// TurboFan valid setFloat32.
let b3 = Math.fround(Math.E); // Round Math.E to float32.
warmup(writeFloat32);
assertOptimized(writeFloat32);
writeFloat32(4, b3);
assertEquals(b3, dataview.getFloat32(4));
writeFloat32(4, b3, true);
assertEquals(b3, dataview.getFloat32(4, true));

// TurboFan valid setFloat64.
let b4 = Math.PI;
warmup(writeFloat64);
assertOptimized(writeFloat64);
writeFloat64(8, b4);
assertEquals(b4, dataview.getFloat64(8));
writeFloat64(8, b4, true);
assertEquals(b4, dataview.getFloat64(8, true));

// TurboFan out of bounds read, deopt.
assertOptimized(writeInt8Handled);
assertInstanceof(writeInt8Handled(24, 0), RangeError);
assertUnoptimized(writeInt8Handled);

// Without exception handler, deopt too.
assertOptimized(writeUint8);
assertThrows(() => writeUint8(24, 0));
assertUnoptimized(writeUint8);

// None of the stores wrote out of bounds.
var bytes = new Uint8Array(buffer);
for (var i = 0; i < 8; i++) assertEquals(0, bytes[i]);
for (var i = 32; i < 64; i++) assertEquals(0, bytes[i]);
