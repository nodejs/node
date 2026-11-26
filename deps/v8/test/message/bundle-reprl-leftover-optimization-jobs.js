// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle-reprl --allow-natives-syntax --turbofan
// Flags: --no-optimize-on-next-call-optimizes-to-maglev
// Flags: --concurrent-recompilation --trace-opt
// With --invoke-weak-callbacks, d8 flushes f's optimization job after the
// script unless a thread is compiling it then: the output order is racy.
// Flags: --no-invoke-weak-callbacks

// JS_BUNDLE_SCRIPT
// Reporting the unhandled rejection runs after d8 drains background jobs.
function f(x) { return x + 1; }
%PrepareFunctionForOptimization(f);
f(1);
Promise.reject({
  toString() {
    // Keep the job pending until this script ends, even if it compiles quickly.
    %DisableOptimizationFinalization();
    %OptimizeFunctionOnNextCall(f, "concurrent");
    f(2);
    return "rejection";
  }
});

// JS_BUNDLE_SCRIPT
print("part 2");
