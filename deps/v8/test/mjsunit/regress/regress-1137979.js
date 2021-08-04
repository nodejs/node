// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --no-lazy-feedback-allocation
// Flags: --noanalyze-environment-liveness

function foo() {
  try {
    bar();
  } catch (e) {}
  for (var i = 0; i < 3; i++) {
    try {
      %PrepareFunctionForOptimization(foo);
      %OptimizeOsr();
    } catch (e) {}
  }
}

foo();
foo();
