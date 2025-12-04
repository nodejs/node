// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function maybe_throw() {}
%NeverOptimizeFunction(maybe_throw);

function with_merge_after_try_catch() {
  var x;
  var inner = function () {
    // This modifies the statically-known parent context.
    x = "foo";
  };
  %PrepareFunctionForOptimization(inner);

  try {
    // Call a function so that we don't eliminate the try-catch.
    maybe_throw();
  } catch (e) {}

  // After this try-catch, we should not have a Phi for the context, else
  // context load-after-store elimination will incorrectly miss the store inside
  // `inner`.

  x = "bar"; // store context[x] = "bar"
  inner();   // store context[x] = "foo" via inner
  return x;  // load context[x]
};
%PrepareFunctionForOptimization(with_merge_after_try_catch);
assertEquals("foo", with_merge_after_try_catch());
assertEquals("foo", with_merge_after_try_catch());
%OptimizeFunctionOnNextCall(with_merge_after_try_catch);
assertEquals("foo", with_merge_after_try_catch());

async function with_merge_after_try_catch_that_has_await() {
  var x;
  var inner = function () {
    // This modifies the statically-known parent context.
    x = "foo";
  };
  %PrepareFunctionForOptimization(inner);

  try {
    // Call a function so that we don't eliminate the try-catch.
    maybe_throw();
  } catch (e) {
    // This will suspend+resume, which switches back to using the on-stack
    // context on resume.
    await 0;
  }

  // After this try-catch, we will have a Phi for the context, between the
  // created function context (if there was no suspend) and the on-stack context
  // (if there was one). Context load-after-store elimination should still not
  // miss the store inside `inner`.

  x = "bar"; // store context[x] = "bar"
  inner();   // store context[x] = "foo" via inner
  return x;  // load context[x]
};

(async function () {
  %PrepareFunctionForOptimization(with_merge_after_try_catch_that_has_await);
  assertEquals("foo", await with_merge_after_try_catch_that_has_await());
  assertEquals("foo", await with_merge_after_try_catch_that_has_await());
  %OptimizeFunctionOnNextCall(with_merge_after_try_catch_that_has_await);
  assertEquals("foo", await with_merge_after_try_catch_that_has_await());
})()


function with_inner_mixed_context_return(return_inside_block) {
  function inner() {
    {
      // Create a block context.
      let x = 5;
      let inner = function () { x; };
      // Return from inside the block context.
      if (return_inside_block) return 1;
    }
    // Return outside the block context.
    return 2;
  }
  %PrepareFunctionForOptimization(inner);

  // There should be no DCHECK failures about context phis from inlining the
  // inner function call, despite that having returns both inside and outside
  // the block context.
  return inner();
};
%PrepareFunctionForOptimization(with_inner_mixed_context_return);
assertEquals(1, with_inner_mixed_context_return(true));
assertEquals(2, with_inner_mixed_context_return(false));
%OptimizeFunctionOnNextCall(with_inner_mixed_context_return);
assertEquals(1, with_inner_mixed_context_return(true));
assertEquals(2, with_inner_mixed_context_return(false));
