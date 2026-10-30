// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-script-context-cells --no-function-context-cells
// Flags: --allow-natives-syntax

let x = 1;
let f = new Function("x = 2; return x;");
%PrepareFunctionForOptimization(f);
assertEquals(2, f());
%OptimizeMaglevOnNextCall(f);
assertEquals(2, f());
