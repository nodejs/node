// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

(function () {
  function ToBigInt(x) {
    return BigInt(x);
  }

  %PrepareFunctionForOptimization(ToBigInt);
  assertEquals(0n, ToBigInt(0));
  %OptimizeFunctionOnNextCall(ToBigInt);

  // Test the builtin ToBigIntConvertNumber.
  assertThrows(() => ToBigInt(undefined), TypeError);

  assertEquals(0n, ToBigInt(false));
  assertEquals(1n, ToBigInt(true));

  assertEquals(42n, ToBigInt(42n));

  assertEquals(3n, ToBigInt(3));
  assertEquals(0xdeadbeefn, ToBigInt(0xdeadbeef));
  assertEquals(-0xdeadbeefn, ToBigInt(-0xdeadbeef));

  assertEquals(2n, ToBigInt("2"));
  assertEquals(0xdeadbeefdeadbeefdn, ToBigInt("0xdeadbeefdeadbeefd"));
  assertThrows(() => ToBigInt("-0x10"), SyntaxError);

  assertThrows(() => ToBigInt(Symbol("foo")), TypeError);
  assertOptimized(ToBigInt);
})();

{
  // Test constants to BigInts.
  function OptimizeAndTest(expected, fun) {
    %PrepareFunctionForOptimization(fun);
    assertEquals(expected, fun());
    %OptimizeFunctionOnNextCall(fun);
    assertEquals(expected, fun());
    assertOptimized(fun);
  }

  OptimizeAndTest(42n, () => BigInt(42n));

  // MinusZero
  OptimizeAndTest(0n, () => BigInt(-0));
  OptimizeAndTest(0n, () => BigInt.asIntN(32, BigInt(-0)));
  OptimizeAndTest(0n, () => BigInt.asUintN(32, BigInt(-0)));
  OptimizeAndTest(0n, () => 0n + BigInt(-0));

  // Smi
  OptimizeAndTest(42n, () => BigInt(42));
  OptimizeAndTest(42n, () => BigInt.asIntN(32, BigInt(42)));
  OptimizeAndTest(42n, () => BigInt.asUintN(32, BigInt(42)));
  OptimizeAndTest(42n, () => 0n + BigInt(42));

  // Signed32
  OptimizeAndTest(-0x80000000n, () => BigInt(-0x80000000));
  OptimizeAndTest(-0x80000000n, () => BigInt.asIntN(32, BigInt(-0x80000000)));
  OptimizeAndTest(0x80000000n, () => BigInt.asUintN(32, BigInt(-0x80000000)));
  OptimizeAndTest(-0x80000000n, () => 0n + BigInt(-0x80000000));

  // Unsigned32
  OptimizeAndTest(0x80000000n, () => BigInt(0x80000000));
  OptimizeAndTest(-0x80000000n, () => BigInt.asIntN(32, BigInt(0x80000000)));
  OptimizeAndTest(0x80000000n, () => BigInt.asUintN(32, BigInt(0x80000000)));
  OptimizeAndTest(0x80000000n, () => 0n + BigInt(0x80000000));
}

(function () {
  function SmiToBigInt(arr) {
    return BigInt(arr[0]);
  }

  // Element kind: PACKED_SMI_ELEMENTS
  const numbers = [0x3fffffff, 0, -0x40000000];
  %PrepareFunctionForOptimization(SmiToBigInt);
  assertEquals(0x3fffffffn, SmiToBigInt(numbers));
  %OptimizeFunctionOnNextCall(SmiToBigInt);
  assertEquals(0x3fffffffn, SmiToBigInt(numbers));
  assertOptimized(SmiToBigInt);

  // Change the map of {numbers}.
  numbers[1] = 0x80000000;
  assertEquals(0x3fffffffn, SmiToBigInt(numbers));
  assertUnoptimized(SmiToBigInt);
})();

(function () {
  function ToBigInt() {
    return BigInt(123);
  }

  %PrepareFunctionForOptimization(ToBigInt);
  assertEquals(123n, ToBigInt());
  %OptimizeFunctionOnNextCall(ToBigInt);
  assertEquals(123n, ToBigInt());
  assertOptimized(ToBigInt);

  // Replace the global BigInt object.
  BigInt = () => 42;
  assertUnoptimized(ToBigInt);
  assertEquals(42, ToBigInt());
})();
