// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --additive-safe-int-feedback
// Flags: --turbofan --no-always-turbofan

const maxAdditiveSafeInteger = 4503599627370495; // 2^52 - 1
const minAdditiveSafeInteger = - 4503599627370496; // - 2^52

// If one of the inputs is a constant in the additive safe range,
// we can use AdditiveSafeInteger.
(function() {
    function foo(a) {
        return a + 1;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertEquals(1231234567891, foo(1231234567890));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertOptimized(foo);

    // We don't deopt in the maximum value.
    assertEquals(maxAdditiveSafeInteger, foo(maxAdditiveSafeInteger - 1));
    assertOptimized(foo);
    // Nor in the minimum.
    assertEquals(minAdditiveSafeInteger + 1, foo(minAdditiveSafeInteger));
    assertOptimized(foo);

    // We deopt if we overflow!
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertOptimized(foo);

    // This time we don't overflow, since we use
    // Float64Add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(2.5, foo(1.5));
    assertOptimized(foo);
})();

// Deopt if input is a double.
(function() {
    function foo(a) {
        return a + 1;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertEquals(1231234567891, foo(1231234567890));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertOptimized(foo);

    // Deopt if input is a double.
    assertEquals(2.5, foo(1.5));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertOptimized(foo);

    // Don't deopt with doubles anymore.
    assertEquals(2.5, foo(1.5));
    assertOptimized(foo);
})();

// If we don't know statically that one of the inputs is
// in the additive safe range, we fallback to float64 add.
(function() {
    function foo(a, b) {
        return a + b;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567891, foo(1231234567890, 1));
    assertEquals(1231234567891, foo(1231234567890, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890, 1));
    assertOptimized(foo);

    // We don't deopt in overflow.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(2.5, foo(1.5, 1));
    assertOptimized(foo);
})();

// We deopt in minus zero.
(function() {
    function foo(a) {
        return a + 1;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertEquals(1231234567891, foo(1231234567890));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertOptimized(foo);

    // TODO(victorgomes): If we know one of the inputs is
    // definitely not minus zero, we could avoid deopting here.
    assertEquals(1, foo(-0));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(2.5, foo(1.5));
    assertOptimized(foo);
})();

// Optimize to AdditiveSafeInteger if one of the inputs is truncated, since it is
// in the safe integer range.
(function() {
    function foo(a, b) {
        return a + (b | 0);
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567891, foo(1231234567890, 1));
    assertEquals(1231234567891, foo(1231234567890, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890, 1));
    assertOptimized(foo);

    // We don't deopt in the maximum value.
    assertEquals(maxAdditiveSafeInteger, foo(maxAdditiveSafeInteger - 1, 1));
    assertOptimized(foo);
    // Nor in the minimum.
    assertEquals(minAdditiveSafeInteger + 1, foo(minAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // We deopt if we overflow!
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger, 1));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891, foo(1231234567890, 1));
    assertOptimized(foo);

    // This time we don't overflow, since we use
    // Float64Add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(2.5, foo(1.5, 1));
    assertOptimized(foo);
})();

// Optimize when the result is pass to another add, since we know the result of
// the add is in the safe integer range.
(function() {
    function foo(a, b) {
        return 1 + a + b;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertEquals(1231234567892, foo(1231234567890, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // We don't deopt in the maximum value.
    assertEquals(maxAdditiveSafeInteger, foo(maxAdditiveSafeInteger - 2, 1));
    assertOptimized(foo);
    // Nor in the minimum.
    assertEquals(minAdditiveSafeInteger + 2, foo(minAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Overflow the second add to ensure that we optimize with additive safe int add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // This time we don't overflow, since we useFloat64Add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertOptimized(foo);

    // But we can still deopt the first add.
    assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // This time no deopts.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(3.5, foo(1.5, 1));
    assertOptimized(foo);
})();

// This also works if the known input is the middle one.
(function() {
    function foo(a, b) {
        return a + 1 + b;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertEquals(1231234567892, foo(1231234567890, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // We don't deopt in the maximum value.
    assertEquals(maxAdditiveSafeInteger, foo(maxAdditiveSafeInteger - 2, 1));
    assertOptimized(foo);
    // Nor in the minimum.
    assertEquals(minAdditiveSafeInteger + 2, foo(minAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Overflow the second add to ensure that we optimize with additive safe int add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // This time we don't overflow, since we useFloat64Add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertOptimized(foo);

    // But we can still deopt the first add.
    assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // This time no deopts.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(3.5, foo(1.5, 1));
    assertOptimized(foo);
})();

// Since add is not an associative operation in JS, unfortunately, we don't
// optimize the first add.
(function() {
    function foo(a, b) {
        return a + b + 1;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertEquals(1231234567892, foo(1231234567890, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // Overflow the second add to ensure that we optimize with additive safe int add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567892, foo(1231234567890, 1));
    assertOptimized(foo);

    // This time we don't overflow, since we use Float64Add.
    assertEquals(maxAdditiveSafeInteger + 1, foo(maxAdditiveSafeInteger - 1, 1));
    assertOptimized(foo);

    // And we cannot deopt by overflowing the first one.
    assertEquals(maxAdditiveSafeInteger + 2, foo(maxAdditiveSafeInteger, 1));
    assertOptimized(foo);

    // Don't deopt with doubles.
    assertEquals(3.5, foo(1.5, 1));
    assertOptimized(foo);
})();

// Don't need to deopt in overflow if we are truncating the result.
(function() {
    function foo(a) {
        return a + 1 | 0;
    }

    %PrepareFunctionForOptimization(foo);
    assertEquals(1231234567891 | 0, foo(1231234567890, 1));
    assertEquals(1231234567891 | 0, foo(1231234567890, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891 | 0, foo(1231234567890, 1));
    assertOptimized(foo);

    // We don't deopt in the maximum value.
    assertEquals(maxAdditiveSafeInteger | 0, foo(maxAdditiveSafeInteger - 1));
    assertOptimized(foo);
    // Nor in the minimum.
    assertEquals(minAdditiveSafeInteger + 1 | 0, foo(minAdditiveSafeInteger));
    assertOptimized(foo);

    // We don't deopt in overflow!
    assertEquals(maxAdditiveSafeInteger + 1 | 0, foo(maxAdditiveSafeInteger));
    assertOptimized(foo);

    // Deopt with doubles.
    assertEquals(2, foo(1.5));
    assertUnoptimized(foo);

    // Optimize again.
    %OptimizeFunctionOnNextCall(foo);
    assertEquals(1231234567891 | 0, foo(1231234567890, 1));
    assertOptimized(foo);

    // Don't deopt with doubles anymore.
    assertEquals(2, foo(1.5, 1));
    assertOptimized(foo);
})();
