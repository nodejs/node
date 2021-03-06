// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax --expose-gc

// Testing GC after setting the pc on the stack.
// There must be a safepoint for the trampoline.

function opt_me() {
  deopt();
}

function deopt() {
  %DeoptimizeFunction(opt_me);
  gc();
}
%NeverOptimizeFunction(deopt);

%PrepareFunctionForOptimization(opt_me);
opt_me();
opt_me();
%OptimizeFunctionOnNextCall(opt_me);
opt_me();
