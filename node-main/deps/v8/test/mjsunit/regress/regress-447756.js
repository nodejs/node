// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function TestConstructor(c) {
  var a = new c(-0);
  assertSame(Infinity, 1 / a.length);
  assertSame(Infinity, 1 / a.byteLength);

  var ab = new ArrayBuffer(-0);
  assertSame(Infinity, 1 / ab.byteLength);

  var a1 = new c(ab, -0, -0);
  assertSame(Infinity, 1 / a1.length);
  assertSame(Infinity, 1 / a1.byteLength);
  assertSame(Infinity, 1 / a1.byteOffset);
}

var constructors =
  [ Uint8Array, Int8Array, Uint8ClampedArray,
    Uint16Array, Int16Array,
    Uint32Array, Int32Array,
    Float32Array, Float64Array ];
for (var i = 0; i < constructors.length; i++) {
  TestConstructor(constructors[i]);
}


function TestOptimizedCode() {
  var a = new Uint8Array(-0);
  assertSame(Infinity, 1 / a.length);
  assertSame(Infinity, 1 / a.byteLength);

  var ab = new ArrayBuffer(-0);
  assertSame(Infinity, 1 / ab.byteLength);

  var a1 = new Uint8Array(ab, -0, -0);
  assertSame(Infinity, 1 / a1.length);
  assertSame(Infinity, 1 / a1.byteLength);
  assertSame(Infinity, 1 / a1.byteOffset);
}

%PrepareFunctionForOptimization(TestOptimizedCode);
TestOptimizedCode();
TestOptimizedCode();
%OptimizeFunctionOnNextCall(TestOptimizedCode);
TestOptimizedCode();
