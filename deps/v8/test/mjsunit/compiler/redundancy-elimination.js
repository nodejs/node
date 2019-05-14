// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeNumberAdd with
// Number feedback.
(function() {
  function bar(i) {
    return ++i;
  }
  bar(0.1);

  function foo(a, i) {
    const x = a[i];
    const y = a[bar(i)];
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2], 0));
  assertEquals(3, foo([1, 2], 0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2], 0));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeNumberAdd with
// NumberOrOddball feedback.
(function() {
  function bar(i) {
    return ++i;
  }
  assertEquals(NaN, bar(undefined));

  function foo(a, i) {
    const x = a[i];
    const y = a[bar(i)];
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2], 0));
  assertEquals(3, foo([1, 2], 0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2], 0));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeNumberSubtract with
// Number feedback.
(function() {
  function bar(i) {
    return --i;
  }
  assertEquals(-0.9, bar(0.1));

  function foo(a, i) {
    const x = a[i];
    const y = a[bar(i)];
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2], 1));
  assertEquals(3, foo([1, 2], 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2], 1));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeNumberSubtract with
// NumberOrOddball feedback.
(function() {
  function bar(i) {
    return --i;
  }
  assertEquals(NaN, bar(undefined));

  function foo(a, i) {
    const x = a[i];
    const y = a[bar(i)];
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2], 1));
  assertEquals(3, foo([1, 2], 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2], 1));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeToNumber.
(function() {
  function foo(a, i) {
    const x = a[i];
    const y = i++;
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo([1, 2], 0));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeSafeIntegerAdd.
(function() {
  function foo(a, i) {
    const x = a[i];
    const y = a[++i];
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2], 0));
  assertEquals(3, foo([1, 2], 0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2], 0));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberOperation()
// TurboFan optimization for the case of SpeculativeSafeIntegerSubtract.
(function() {
  function foo(a, i) {
    const x = a[i];
    const y = a[--i];
    return x + y;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2], 1));
  assertEquals(3, foo([1, 2], 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2], 1));
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberComparison()
// TurboFan optimization for the case of SpeculativeNumberEqual.
(function() {
  function foo(a, i) {
    const x = a[i];
    if (i === 0) return x;
    return i;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 1));
  // Even passing -0 should not deoptimize and
  // of course still pass the equality test above.
  assertEquals(9, foo([9, 2], -0));
  assertOptimized(foo);
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberComparison()
// TurboFan optimization for the case of SpeculativeNumberLessThan.
(function() {
  function foo(a, i) {
    const x = a[i];
    if (i < 1) return x;
    return i;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 1));
  // Even passing -0 should not deoptimize and
  // of course still pass the equality test above.
  assertEquals(9, foo([9, 2], -0));
  assertOptimized(foo);
})();

// Test the RedundancyElimination::ReduceSpeculativeNumberComparison()
// TurboFan optimization for the case of SpeculativeNumberLessThanOrEqual.
(function() {
  function foo(a, i) {
    const x = a[i];
    if (i <= 0) return x;
    return i;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo([1, 2], 0));
  assertEquals(1, foo([1, 2], 1));
  // Even passing -0 should not deoptimize and
  // of course still pass the equality test above.
  assertEquals(9, foo([9, 2], -0));
  assertOptimized(foo);
})();
