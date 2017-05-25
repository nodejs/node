// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo

(function MaterializeStrictArguments() {
  "use strict"

  function f(x, y) {
    return x + y;
  }

  function test() {
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  assertEquals(test(1, 2), 3);
  assertEquals(test(1, 2, 3), 3);

  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2), 3);
  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2, 3), 3);
})();

(function MaterializeSloppyArguments() {
  function f(x, y) {
    return x + y;
  }

  function test() {
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  assertEquals(test(1, 2), 3);
  assertEquals(test(1, 2, 3), 3);

  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2), 3);
  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2, 3), 3);
})();

(function MaterializeStrictOverwrittenArguments() {
  "use strict"

  function f(x, y) {
    return x + y;
  }

  function test(a, b) {
    a = 4;
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  assertEquals(test(1, 2), 3);
  assertEquals(test(1, 2, 3), 3);

  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2), 3);
  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2, 3), 3);
})();

(function MaterializeSloppyOverwrittenArguments() {
  function f(x, y) {
    return x + y;
  }

  function test(a, b) {
    a = 4;
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  test(1, 2);
  test(3, 4, 5);

  assertEquals(test(1, 2), 6);
  assertEquals(test(1, 2, 3), 6);

  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2), 6);
  %OptimizeFunctionOnNextCall(test);
  assertEquals(test(1, 2, 3), 6);
})();
