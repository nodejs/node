// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0

function bar() {
  try { fail(); } catch(e) {}
}
%PrepareFunctionForOptimization(bar);

// Simple phi pointing to the call result
// at input index 2 (last).
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      p = bar();
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// Simple phi pointing to a dead use
// at input index 2 (last).
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      bar();
      p = 2;
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// Phi pointing to the call result
// at input index 1.
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      p = bar();
    } else {
      p = 2;
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// Phi pointing to a dead use
// at input index 1.
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      bar();
      p = 2;
    } else {
      p = 3;
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// Phi pointing to a dead use
// at input index 1 with nested blocks.
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      bar();
      if (a == 42) {
        p = 1;
      } else {
        p = 2;
      }
    } else {
      p = 3;
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// Loop phi pointing to the call result.
(function() {
  function foo(a) {
    let p = 0;
    for (let i = 0; i < 5; i++) {
      if (a < 3) {
        p = bar();
      }
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();


// Loop phi pointing to a dead use.
(function() {
  function foo(a) {
    let p = 0;
    for (let i = 0; i < 5; i++) {
      if (a < 3) {
        bar();
        p = i + 2;
      }
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// Loop phi pointing to a call result
// with index 2
(function() {
  function foo(a) {
    let p = 0;
    for (let i = 0; i < 5; i++) {
      if (a < 3) {
        p = bar();
      } else {
        p = 3;
      }
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// 3-value Phi pointing to the call result
// at input index 1.
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      p = bar()
    } else if (a == 4) {
      p = 2;
    } else {
      p = 3;
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  // This makes sure that the call has a high call frequency.
  for (let i = 0; i < 20; i++) {
    foo(1);
  }
  foo(4);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// 3-value Phi pointing to the call result
// at input index 2.
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      p = 1;
    } else if (a == 4) {
      p = bar();
    } else {
      p = 3;
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  // This makes sure that the call has a high call frequency.
  for (let i = 0; i < 20; i++) {
    foo(4);
  }
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();

// 3-value Phi pointing to the call result
// at input index 3.
(function() {
  function foo(a) {
    let p = 0;
    if (a < 3) {
      p = 1;
    } else if (a == 4) {
      p = 2;
    } else {
      p = bar();
    }
    return p;
  }
  %PrepareFunctionForOptimization(foo);
  // This makes sure that the call has a high call frequency.
  for (let i = 0; i < 20; i++) {
    foo(5);
  }
  foo(1);
  %OptimizeMaglevOnNextCall(foo);
  foo(1);
})();
