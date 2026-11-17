// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax

let sink = 0;
function* gen() {
  try { unknown_global; } catch {}
  let x = 0;
  for (;;) {
    if (x === 0) {
      sink = 1;
    } else {
      sink = 2;
    }
    const is_zero = x === 0 ? true : false;
    x = (yield is_zero).length;
  }
}

%PrepareFunctionForOptimization(gen);
const g = gen();
g.next();     // x = 0
g.next("a");  // x = 1
g.next("");   // x = 0

%OptimizeFunctionOnNextCall(gen);
assertEquals(false, g.next("hello").value);
