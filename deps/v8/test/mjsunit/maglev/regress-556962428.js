// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* gen(x) {
  /1/.test(x);
  for (let i = 0; i < x.length; ++i) {
    for (let j = 0; j < 1; j++) {
      x--;
    }
  }
  yield x;
}
%PrepareFunctionForOptimization(gen);

let g = gen(1);
assertEquals({value: 1, done: false}, g.next());

%OptimizeMaglevOnNextCall(gen);
let g2 = gen(1);
assertOptimized(gen);
assertEquals({value: 1, done: false}, g2.next());
assertEquals({value: undefined, done: true}, g2.next());
