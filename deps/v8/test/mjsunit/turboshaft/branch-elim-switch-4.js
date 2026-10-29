// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(x) {
  x |= 0; // So that {x} is treated as a Word32.
  if (x) {
    switch(x) {
      case 1: return 0;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      default:
        return 42;
    }
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(2));
assertEquals(42, foo(2));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(2));
