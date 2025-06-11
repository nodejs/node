// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-optimize-apply --turbofan
// Flags: --no-always-turbofan

// These tests do not work well if we flush the feedback vector, which causes
// deoptimization.
// Flags: --no-stress-flush-code --no-flush-bytecode

// Some of the tests rely on optimizing/deoptimizing at predictable moments, so
// this is not suitable for deoptimization fuzzing.
// Flags: --deopt-every-n-times=0

// Test for optimization of CallWithSpread when the array iterator is replaced
// with a generator function and the array in empty.
//
// Note: this test must be in a separate file because the test invalidates a
// protector, which then remains invalidated.
(function () {
  "use strict";

  // This invalidates the DependOnArrayIteratorProtector.
  Object.defineProperty(Array.prototype, Symbol.iterator, {
    value: function* () {
      yield 42;
    },
  });

  // Introduce an indirection, so that we don't depend on
  // ContextCells constness.
  var log_got_interpreted = null;
  log_got_interpreted = true;

  function log(a) {
    assertEquals(1, arguments.length);
    log_got_interpreted = %IsBeingInterpreted();
    return a;
  }
  function foo() {
    return log(...[]);
  }

  %PrepareFunctionForOptimization(log);
  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo());
  assertTrue(log_got_interpreted);

  // Compile foo.
  %OptimizeFunctionOnNextCall(log);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42, foo());
  // The call with spread should not have been inlined, because of the
  // generator/iterator.
  assertFalse(log_got_interpreted);
  assertOptimized(foo);
})();
