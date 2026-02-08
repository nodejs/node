// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function foo(ta) {
  return ta.length;
}
%PrepareFunctionForOptimization(foo);

// Invalidate the ArrayBufferDetached protector before compiling.
let ab1 = new ArrayBuffer(8);
%ArrayBufferDetach(ab1);

let ab2 = new ArrayBuffer(8);
let ta = new Uint32Array(ab2);
assertEquals(2, foo(ta));

%OptimizeFunctionOnNextCall(foo);
assertEquals(2, foo(ta));
assertOptimized(foo);

// If ab2 gets detached, we don't deopt.
%ArrayBufferDetach(ab2);
assertOptimized(foo);

// But we deopt if the detached AB is actually accessed.
assertEquals(0, foo(ta));
assertUnoptimized(foo);
