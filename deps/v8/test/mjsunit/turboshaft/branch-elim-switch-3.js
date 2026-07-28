// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(x) {
  // We need {x} to be a Word32 so that `if (x)` isn't a TaggedEqual comparison.
  x |= 0;

  if (x) {
    switch (x) {
      case 0:
          %TurbofanStaticAssert(false);
      case 1:
      case 25:
      case 54:
      default:
        return 42;
    }
  } else {
    switch (x) {
      case 0:
        return 17;
      case 1:
      case 25:
      case 54:
          %TurbofanStaticAssert(false);
      default:
          %TurbofanStaticAssert(false);
    }
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals(17, foo(0));
assertEquals(42, foo(1));

%OptimizeFunctionOnNextCall(foo);
assertEquals(17, foo(0));
assertEquals(42, foo(1));
