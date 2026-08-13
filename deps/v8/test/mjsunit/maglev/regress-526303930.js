// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-maglev --fuzzing

function f(a, b) {
   return a + b;
}
%PrepareFunctionForOptimization(f);
%OptimizeMaglevOnNextCall(f);
f(1);
