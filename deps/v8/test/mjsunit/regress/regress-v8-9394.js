// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

(function testMaybeAssignedWithShadowing() {

  function foo() {
    let a = 0;
    let g;

    with ({}) {
      g = function g() {
        // Increment a, should set it maybe_assigned but doesn't in the bug.
        ++a;
      }
      // Shadowing the outer 'a' with a dynamic one.
      a;
    }

    return function () {
      // The access to a would be context specialized (to 2 since it's after the
      // second call) if maybe_assigned were incorrectly not set.
      g(a);
      return a;
    }
  };

  f = foo();
  %PrepareFunctionForOptimization(f);
  assertEquals(f(), 1);
  assertEquals(f(), 2);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f(), 3);

})();

// Same test as above, just with more shadowing (including dynamic->dynamic
// shadowing) and skipping over scopes with shadows.
(function testMaybeAssignedWithDeeplyNestedShadowing() {

  function foo() {
    let a = 0;
    let g;

    // Increment a, should set it maybe_assigned but doesn't in the bug.
    with ({}) {
      with ({}) {
        with ({}) {
          with ({}) {
            with ({}) {
              g = function g() { ++a; }
              // Shadow the second dynamic 'a'.
              a;
            }
            // Shadowing the first dynamic 'a'.
            a;
          }
          // Skip shadowing here
        }
        // Skip shadowing here
      }
      // Shadowing the outer 'a' with a dynamic one.
      a;
    }

    return function () {
      // The access to a would be context specialized (to 2 since it's after the
      // second call) if maybe_assigned were incorrectly not set.
      g(a);
      return a;
    }
  };

  f = foo();
  %PrepareFunctionForOptimization(f);
  assertEquals(f(), 1);
  assertEquals(f(), 2);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(f(), 3);

})();
