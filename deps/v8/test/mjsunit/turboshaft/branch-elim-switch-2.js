// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(x) {
  // We need {x} to be a Word32 so that `if (x)` isn't a TaggedEqual comparison.
  x |= 0;

  switch (x) {
    case 0:
      switch (x) {
      case 0:
        return 42;
      case 1:
      case 25:
      case 54:
        %TurbofanStaticAssert(false);
      default:
        %TurbofanStaticAssert(false);
      }
      break;

    case 1:
      switch (x) {
      case 0:
        %TurbofanStaticAssert(false);
      case 1:
        return 17;
      case 25:
      case 54:
        %TurbofanStaticAssert(false);
      default:
        %TurbofanStaticAssert(false);
      }
      break;

    case 2:
      switch (x) {
      case 0:
      case 1:
        %TurbofanStaticAssert(false);
      case 2:
        return 23;
      case 25:
      case 54:
        %TurbofanStaticAssert(false);
      default:
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
