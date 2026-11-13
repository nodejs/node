// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* genWithDeadTemps(x) {
  let a = x + 1;
  let b = { val: a * 2 };
  let c = [b.val, x + 3, x + 4];
  let live1 = c[0] + c[1] + c[2];
  // a, b, c are dead across this yield.
  let resumed = yield live1;
  let d = resumed * 3;
  let e = { nested: d + 5 };
  let live2 = live1 + e.nested;
  // a, b, c, d, e, live1 are dead across this second yield.
  let final_in = yield live2;
  return live2 + final_in;
}

%PrepareFunctionForOptimization(genWithDeadTemps);
for (let i = 0; i < 5; ++i) {
  let g = genWithDeadTemps(i);
  assertEquals({ value: 4 * i + 9, done: false }, g.next());
  assertEquals({ value: 4 * i + 9 + (i * 3 + 5), done: false }, g.next(i));
  assertEquals({ value: 7 * i + 14 + 10, done: true }, g.next(10));
}
%OptimizeFunctionOnNextCall(genWithDeadTemps);
for (let i = 0; i < 5; ++i) {
  let g = genWithDeadTemps(i);
  assertEquals({ value: 4 * i + 9, done: false }, g.next());
  assertEquals({ value: 4 * i + 9 + (i * 3 + 5), done: false }, g.next(i));
  assertEquals({ value: 7 * i + 14 + 10, done: true }, g.next(10));
}
