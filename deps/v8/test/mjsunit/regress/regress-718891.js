// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function Data() {
}
Data.prototype = { x: 1 };

function TriggerDeopt() {
  Data.prototype = { x: 2 };
}

function TestDontSelfHealWithDeoptedCode(run_unoptimized, ClosureFactory) {
  // Create some function closures which don't have
  // optimized code.
  var unoptimized_closure = ClosureFactory();
  if (run_unoptimized) {
    unoptimized_closure();
  }

  // Run and optimize the code (do this in a separate function
  // so that the closure doesn't leak in a dead register).
  (() => {
    var optimized_closure = ClosureFactory();
    %PrepareFunctionForOptimization(optimized_closure);
    // Use .call to avoid the CallIC retaining the JSFunction in the
    // feedback vector via a weak map, which would mean it wouldn't be
    // collected in the minor gc below.
    optimized_closure.call(undefined);
    %OptimizeFunctionOnNextCall(optimized_closure);
    optimized_closure.call(undefined);
  })();

  // Optimize a dummy function, just so it gets linked into the
  // Contexts optimized_functions list head, which is in the old
  // space, and the link from to the optimized_closure's JSFunction
  // moves to the inline link in dummy's JSFunction in the new space,
  // otherwise optimized_closure's JSFunction will be retained by the
  // old->new remember set.
  (() => {
    var dummy = function() { return 1; };
    %PrepareFunctionForOptimization(dummy);
    %OptimizeFunctionOnNextCall(dummy);
    dummy();
  })();

  // GC the optimized closure with a minor GC - the optimized
  // code will remain in the feedback vector.
  gc(true);

  // Trigger deoptimization by changing the prototype of Data. This
  // will mark the code for deopt, but since no live JSFunction has
  // optimized code, we won't clear the feedback vector.
  TriggerDeopt();

  // Call pre-existing functions, these will try to self-heal with the
  // optimized code in the feedback vector op, but should bail-out
  // since the code is marked for deoptimization.
  unoptimized_closure();
}

// Run with the unoptimized closure both uncomplied and compiled for the
// interpreter initially, to test self healing on both CompileLazy and
// the InterpreterEntryTrampoline respectively.
TestDontSelfHealWithDeoptedCode(false,
        () => { return () => { return new Data() }});
TestDontSelfHealWithDeoptedCode(true,
        () => { return () => { return new Data() }});
