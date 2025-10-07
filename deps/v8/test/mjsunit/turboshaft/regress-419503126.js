// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

function foo() {
  for (let i = 0; i < 42;) {
    i++;
    try {
      for (let j = 0; j < 10; j++) {
        // empty
      }
      return 42;
    } catch(e) {
      // Not returning, so this is the only way to actually loop. This is
      // unreachable because nothing can actually throw within the catch, but
      // Turbofan's stack checks are marked as throwing, which means that
      // Turbofan will wire the loop stack checks of the inner loop this catch
      // handler, even though the stack check will never throw.  As a result,
      // the outer loop doesn't actually loop, but Turbofan doesn't realize this
      // (Turboshaft does though).
    }
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo());
