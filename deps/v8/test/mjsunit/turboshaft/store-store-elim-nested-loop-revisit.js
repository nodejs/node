// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --disable-loop-stack-checks

function foo(o) {
  o.x = 111;

  for (let i = 0; i < 42; i++) {
    o.y = {x: o.y};
    o.x = 17;

    for (let j = 0; j < 17; j++) {
      o.x = 25;
    }
  }

  o.x = 19;
}

let o = {x: 2, y: 'abc'};
o.x = 43;
o.y = 'x';

%PrepareFunctionForOptimization(foo);
foo(o);

%OptimizeFunctionOnNextCall(foo);
foo(o);
assertEquals(19, o.x);
