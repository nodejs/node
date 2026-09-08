// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function test() {
  for (let i = 0; i < 5; i++) {
    function inner(x) {
      try {
        let table = Object.create(null);
        while (true) {
          // Here x is an unsigned, untagged word32 (from `2931037698 >>> i`).
          x ^= 0;
          // `x ^= 0` is a TruncateJSPrimitiveToWord32OrDeopt of a
          // ConvertUntaggedToJSPrimitive(x); afterwards x is a *signed* word32.
          // Both representations reach the catch handler's exception phi for x,
          // even though a reducer collapses them to a single Turboshaft value.
          for (let _ of table) {
            // Throws: `table` has a null prototype and is not iterable.
          }
        }
      } catch (e) {
        for (let k = 0; k < 1; k++) {
        }
      }
    }
    inner(2931037698 >>> i);
  }
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
