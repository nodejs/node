// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --flush-bytecode --concurrent-recompilation
// Flags: --maglev --turbofan

function test(f, optimize, recompile) {
  %PrepareFunctionForOptimization(f);
  f(1, 2);
  f(2, 3);
  %DisableOptimizationFinalization();
  optimize(f);
  // No stack checks (JS calls/loops) may happen between this call and
  // %ForceFlush below: Maglev jobs are finalized by a stack-guard interrupt
  // that %DisableOptimizationFinalization does not suppress.
  f(3, 4);
  %WaitForBackgroundOptimization();
  // Flush the bytecode and feedback vector while the job awaits finalization.
  %ForceFlush(f);
  // Optionally recompile lazily, which only reallocates the closure feedback
  // cell array, not the feedback vector.
  if (recompile) f(4, 5);
  %FinalizeOptimization();
  assertEquals(9, f(4, 5));
}

function optimizeMaglev(f) { %OptimizeMaglevOnNextCall(f, "concurrent"); }
function optimizeTurbofan(f) { %OptimizeFunctionOnNextCall(f, "concurrent"); }

function maglev1(a, b) { return a + b; }
test(maglev1, optimizeMaglev, false);
function maglev2(a, b) { return a + b; }
test(maglev2, optimizeMaglev, true);

function turbofan1(a, b) { return a + b; }
test(turbofan1, optimizeTurbofan, false);
function turbofan2(a, b) { return a + b; }
test(turbofan2, optimizeTurbofan, true);
