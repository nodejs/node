// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(x) {
  // We need {x} to be a Word32 so that `if (x)` isn't a TaggedEqual comparison.
  x |= 0;

  switch (x) {
    case 0:
      if (x) {
        %TurbofanStaticAssert(false);
      } else {
        return 42;
      }
      break;
    case 1:
      if (x) {
        return 17;
      } else {
        %TurbofanStaticAssert(false);
      }
      break;
    case 2:
      if (x) {
        return 23;
      } else {
        %TurbofanStaticAssert(false);
      }
      break;
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(0));
assertEquals(17, foo(1));
assertEquals(23, foo(2));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(0));
assertEquals(17, foo(1));
assertEquals(23, foo(2));
