// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function foo(b, a) {
  switch (b) {
    case 1:
      let x = 10;
      let val = a & 0xF;
      if (val <= 16) {
        return 0;
      }
      // Fallthrough
    case 2:
      return x;
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(1, 1));
assertThrows(() => foo(2, 1), ReferenceError,
             "Cannot access 'x' before initialization");

%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo(2, 1), ReferenceError,
             "Cannot access 'x' before initialization");
