// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan

function foo(size) {
  let a = new Uint8Array(size);
  let s = "hello";
  return s.startsWith("h", a.length);
}
%PrepareFunctionForOptimization(foo);
foo(0);
%OptimizeFunctionOnNextCall(foo);
foo(0);

assertTrue(foo(0));
assertFalse(foo(1));
assertOptimized(foo);
