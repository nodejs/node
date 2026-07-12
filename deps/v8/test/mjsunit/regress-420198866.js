// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-alloaction --no-lazy

function foo() {
  try {
    const v = class {}();
  } catch (e) {
    print('boom');
  }
}
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
