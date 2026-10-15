// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax

let sink = 0;
const arr = [1.1, 2.2];
const victim = [3.3];

function* gen() {
  try { unknown_global; } catch {}
  let x = 0;
  for (;;) {
    if (x === 0) {
      sink = 1;
    } else {
      sink = 2;
    }
    if (arr.length <= 0) return;
    arr[x] = 1.5;
    x = (yield).length;
  }
}

%PrepareFunctionForOptimization(gen);
const g = gen();
g.next();     // x = 0
g.next("a");  // x = 1
g.next("");   // x = 0

%OptimizeFunctionOnNextCall(gen);
g.next("abcdef");
assertEquals(3.3, victim[0]);
assertEquals(7, arr.length);
