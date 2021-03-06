// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax
// Flags: --nostress-opt --opt

// Ensure that ElementsKind transitions in various situations are hoisted (or
// not hoisted) correctly, don't change the semantics programs and don't trigger
// deopt through hoisting in important situations.

function test_wrapper() {
  // Make sure that a simple elements array transitions inside a loop before
  // stores to an array gets hoisted in a way that doesn't generate a deopt in
  // simple cases.}
  function testDoubleConversion4(a) {
    var object = new Object();
    a[0] = 0;
    var count = 3;
    do {
      a[0] = object;
    } while (--count > 0);
  }

  %PrepareFunctionForOptimization(testDoubleConversion4);
  testDoubleConversion4(new Array(5));
  testDoubleConversion4(new Array(5));  // Call twice to make sure that second
                                        // store is a transition and not
                                        // optimistically MONOMORPHIC
  %OptimizeFunctionOnNextCall(testDoubleConversion4);
  testDoubleConversion4(new Array(5));
  testDoubleConversion4(new Array(5));
  assertOptimized(testDoubleConversion4);
  %ClearFunctionFeedback(testDoubleConversion4);

  // Make sure that non-element related map checks that are not preceded by
  // transitions in a loop still get hoisted in a way that doesn't generate a
  // deopt in simple cases.
  function testExactMapHoisting(a) {
    var object = new Object();
    a.foo = {};
    a[0] = 0;
    a[1] = 1;
    var count = 3;
    do {
      a.foo = object;  // This map check should be hoistable
      a[1] = object;
      result = a.foo == object && a[1] == object;
    } while (--count > 0);
  }

  %PrepareFunctionForOptimization(testExactMapHoisting);
  testExactMapHoisting(new Array(5));
  testExactMapHoisting(new Array(5));  // Call twice to make sure that second
                                       // store is a transition and not
                                       // optimistically MONOMORPHIC
  %OptimizeFunctionOnNextCall(testExactMapHoisting);
  testExactMapHoisting(new Array(5));
  testExactMapHoisting(new Array(5));
  assertOptimized(testExactMapHoisting);
  %ClearFunctionFeedback(testExactMapHoisting);

  // Make sure that non-element related map checks do NOT get hoisted if they
  // depend on an elements transition before them and it's not possible to hoist
  // that transition.
  function testExactMapHoisting2(a) {
    var object = new Object();
    a.foo = 0;
    a[0] = 0;
    a[1] = 1;
    var count = 3;
    do {
      if (a.bar === undefined) {
        a[1] = 2.5;
      }
      a.foo = object;  // This map check should NOT be hoistable because it
                       // includes a check for the PACKED_ELEMENTS map as well as
                       // the PACKED_DOUBLE_ELEMENTS map, which depends on the
                       // double transition above in the if, which cannot be
                       // hoisted.
    } while (--count > 0);
  }

  %PrepareFunctionForOptimization(testExactMapHoisting2);
  testExactMapHoisting2(new Array(5));
  testExactMapHoisting2(new Array(5));  // Call twice to make sure that second
                                        // store is a transition and not
                                        // optimistically MONOMORPHIC
  %OptimizeFunctionOnNextCall(testExactMapHoisting2);
  testExactMapHoisting2(new Array(5));
  testExactMapHoisting2(new Array(5));
  // Temporarily disabled - see bug 2176.
  // assertOptimized(testExactMapHoisting2);
  %ClearFunctionFeedback(testExactMapHoisting2);

  // Make sure that non-element related map checks do get hoisted if they use
  // the transitioned map for the check and all transitions that they depend
  // upon can hoisted, too.
  function testExactMapHoisting3(a) {
    var object = new Object();
    a.foo = null;
    a[0] = 0;
    a[1] = 1;
    var count = 3;
    do {
      a[1] = 2.5;
      a.foo = object;  // This map check should be hoistable because all elements
                       // transitions in the loop can also be hoisted.
    } while (--count > 0);
  }

  %PrepareFunctionForOptimization(testExactMapHoisting3);
  var add_transition = new Array(5);
  add_transition.foo = 0;
  add_transition[0] = new Object();  // For FAST_ELEMENT transition to be created
  testExactMapHoisting3(new Array(5));
  testExactMapHoisting3(new Array(5));  // Call twice to make sure that second
                                        // store is a transition and not
                                        // optimistically MONOMORPHIC
  %OptimizeFunctionOnNextCall(testExactMapHoisting3);
  testExactMapHoisting3(new Array(5));
  testExactMapHoisting3(new Array(5));
  assertOptimized(testExactMapHoisting3);
  %ClearFunctionFeedback(testExactMapHoisting3);

  function testDominatingTransitionHoisting1(a) {
    var object = new Object();
    a[0] = 0;
    var count = 3;
    do {
      if (a.baz != true) {
        a[1] = 2.5;
      }
      a[0] = object;
    } while (--count > 3);
  }

  /*
  %PrepareFunctionForOptimization(testDominatingTransitionHoisting1);
  testDominatingTransitionHoisting1(new Array(5));
  testDominatingTransitionHoisting1(new Array(5));  // Call twice to make sure
                                                    // that second store is a
                                                    // transition and not
                                                    // optimistically MONOMORPHIC
  %OptimizeFunctionOnNextCall(testDominatingTransitionHoisting1);
  testDominatingTransitionHoisting1(new Array(5));
  testDominatingTransitionHoisting1(new Array(5));
  // TODO(verwaest) With current changes the elements transition gets hoisted
  // above the access, causing a deopt. We should update the type of access
  // rather than forbid hoisting the transition.
  assertOptimized(testDominatingTransitionHoisting1);
  %ClearFunctionFeedback(testDominatingTransitionHoisting1);
  */

  function testHoistingWithSideEffect(a) {
    var object = new Object();
    a[0] = 0;
    var count = 3;
    do {
      assertTrue(true);
      a[0] = object;
    } while (--count > 3);
  }

  %PrepareFunctionForOptimization(testHoistingWithSideEffect);
  testHoistingWithSideEffect(new Array(5));
  testHoistingWithSideEffect(new Array(5));  // Call twice to make sure that
                                             // second store is a transition and
                                             // not optimistically MONOMORPHIC
  %OptimizeFunctionOnNextCall(testHoistingWithSideEffect);
  testHoistingWithSideEffect(new Array(5));
  testHoistingWithSideEffect(new Array(5));
  assertOptimized(testHoistingWithSideEffect);
  %ClearFunctionFeedback(testHoistingWithSideEffect);

  function testStraightLineDupeElinination(a,b,c,d,e,f) {
    var count = 3;
    do {
      assertTrue(true);
      a[0] = b;
      a[1] = c;
      a[2] = d;
      assertTrue(true);
      a[3] = e;  // TransitionElementsKind should be eliminated despite call.
      a[4] = f;
    } while (--count > 3);
  }

  %PrepareFunctionForOptimization(testStraightLineDupeElinination);
  testStraightLineDupeElinination(new Array(0, 0, 0, 0, 0),0,0,0,0,.5);
  testStraightLineDupeElinination(new Array(0, 0, 0, 0, 0),0,0,0,.5,0);
  testStraightLineDupeElinination(new Array(0, 0, 0, 0, 0),0,0,.5,0,0);
  testStraightLineDupeElinination(new Array(0, 0, 0, 0, 0),0,.5,0,0,0);
  testStraightLineDupeElinination(new Array(0, 0, 0, 0, 0),.5,0,0,0,0);
  testStraightLineDupeElinination(new Array(.1,.1,.1,.1,.1),0,0,0,0,.5);
  testStraightLineDupeElinination(new Array(.1,.1,.1,.1,.1),0,0,0,.5,0);
  testStraightLineDupeElinination(new Array(.1,.1,.1,.1,.1),0,0,.5,0,0);
  testStraightLineDupeElinination(new Array(.1,.1,.1,.1,.1),0,.5,0,0,0);
  testStraightLineDupeElinination(new Array(.1,.1,.1,.1,.1),.5,0,0,0,0);
  testStraightLineDupeElinination(new Array(5),.5,0,0,0,0);
  testStraightLineDupeElinination(new Array(5),0,.5,0,0,0);
  testStraightLineDupeElinination(new Array(5),0,0,.5,0,0);
  testStraightLineDupeElinination(new Array(5),0,0,0,.5,0);
  testStraightLineDupeElinination(new Array(5),0,0,0,0,.5);
  testStraightLineDupeElinination(new Array(5),.5,0,0,0,0);
  testStraightLineDupeElinination(new Array(5),0,.5,0,0,0);
  testStraightLineDupeElinination(new Array(5),0,0,.5,0,0);
  testStraightLineDupeElinination(new Array(5),0,0,0,.5,0);
  testStraightLineDupeElinination(new Array(5),0,0,0,0,.5);
  %OptimizeFunctionOnNextCall(testStraightLineDupeElinination);
  testStraightLineDupeElinination(new Array(5),0,0,0,0,0);
  testStraightLineDupeElinination(new Array(5),0,0,0,0,0);
  assertOptimized(testStraightLineDupeElinination);
  %ClearFunctionFeedback(testStraightLineDupeElinination);
}

// The test is called in a test wrapper that has type feedback cleared to
// prevent the influence of allocation-sites, which learn from transitions.
test_wrapper();
%ClearFunctionFeedback(test_wrapper);
