// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --interrupt-budget=100 --budget-for-feedback-vector-allocation=10 --allow-natives-syntax

function f() {
  let s = 0;
  for (let i = 0; i < 10; i++) {
    s += i;
  }
  return s;
}

%PrepareFunctionForOptimization(f, "allow heuristic optimization");
f();
f();
f();
assertOptimized(f);
