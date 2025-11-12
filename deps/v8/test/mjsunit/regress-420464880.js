// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-alloaction --no-lazy

bar = undefined;
function foo() {
  try {
    let i = 0;
    let v;
    bar = function () { i++; };
    while (i < 6) {
      bar();
      v = i;
      if (i == 5) %OptimizeOsr();
    }
    return v;
  } catch (e) {}
}
%PrepareFunctionForOptimization(foo);
foo();
assertEquals(6, foo());
