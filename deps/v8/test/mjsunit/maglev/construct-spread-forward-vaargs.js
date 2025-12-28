// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let result;

class Z {
  constructor() {
    result = arguments[1];
  }
}

// Arguments creation is elided, we forward varargs.
(function() {
    class Q extends Z {
    constructor(){
        super(...arguments);
        arguments
    }
    }

    %PrepareFunctionForOptimization(Q);
    %PrepareFunctionForOptimization(Z);
    result = 0;
    new Q(1, 2, 3);
    assertEquals(2, result);

    %OptimizeFunctionOnNextCall(Q);
    result = 0;
    new Q(1, 2, 3);
    assertEquals(2, result);
})();

// Update arguments beforehanded (no forward varargs)
(function() {
    class Q extends Z {
    constructor(){
        arguments[1] = 42;
        super(...arguments);
        arguments
    }
    }

    %PrepareFunctionForOptimization(Q);
    %PrepareFunctionForOptimization(Z);
    result = 0;
    new Q(1, 2, 3);
    assertEquals(42, result);

    %OptimizeFunctionOnNextCall(Q);
    result = 0;
    new Q(1, 2, 3);
    assertEquals(42, result);
})();

// Update arguments afterwards (forward varargs is allowed)
(function() {
    class Q extends Z {
    constructor(){
        super(...arguments);
        arguments[1] = 42;
    }
    }

    %PrepareFunctionForOptimization(Q);
    %PrepareFunctionForOptimization(Z);
    result = 0;
    new Q(1, 2, 3);
    assertEquals(2, result);

    %OptimizeFunctionOnNextCall(Q);
    result = 0;
    new Q(1, 2, 3);
    assertEquals(2, result);
})();
