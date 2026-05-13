// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --for-of-optimization

const iterable = {
  [Symbol.iterator]() {
    return {
      next() {
        // Implicitly returns undefined.
        // This triggers the eager deoptimization continuation which must
        // cleanly throw "Iterator result is not an object" without crashing.
      }
    };
  }
};

function test() {
  try {
    for (const x of iterable) {}
  } catch (e) {}
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeMaglevOnNextCall(test);
test();
