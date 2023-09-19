// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --interrupt-budget=100 --interrupt-budget-for-feedback-allocation=10 --allow-natives-syntax --nomaglev

function f() {
  let s = 0;
  for (let i = 0; i < 10; i++) {
    s += i;
  }
  return s;
}

%EnsureFeedbackVectorForFunction(f);
f();
f();
f();
%FinalizeOptimization();
assertOptimized(f);
