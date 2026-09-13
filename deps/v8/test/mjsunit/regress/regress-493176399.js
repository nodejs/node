// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --track-array-buffer-views

function foo(obj) {
  return obj.p;
}

let ab1 = new ArrayBuffer();
let ta1 = new Int32Array(ab1);
%ArrayBufferDetach(ab1);
ta1.p = 1;
%PrepareFunctionForOptimization(foo);
assertEquals(1, foo(ta1));

let ab2 = new ArrayBuffer();
let ta2 = new Int32Array(ab2);
%ArrayBufferDetach(ab2);
ta2.p = 1.1;
%OptimizeFunctionOnNextCall(foo);
assertEquals(1.1, foo(ta2));
