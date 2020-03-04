// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const obj = { a: 42 };

function boom() {
  throw "boom";
}

// Ensure that we optimize the monomorphic case.
(function() {
  function bar(x) {
    try {
      boom();
      ++i;
    } catch(_) {
      %TurbofanStaticAssert(x.a == 42);
      return x.a;
    }
  }

  function foo() {
    return bar(obj);
  }

  %PrepareFunctionForOptimization(foo);
  %PrepareFunctionForOptimization(bar);

  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

// And the megamorphic case.
(function() {
  function bar(x) {
    try {
      boom();
      ++i;
    } catch(_) {
      %TurbofanStaticAssert(x.a == 42);
      return x.a;
    }
  }

  function foo() {
    return bar(obj);
  }

  %PrepareFunctionForOptimization(foo);
  %PrepareFunctionForOptimization(bar);

  bar({b: 42});
  bar({c: 42});
  bar({d: 42});
  bar({e: 42});
  bar({f: 42});

  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();
