// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev --turbolev-escape-analysis

function MyObj() {
  this.x = 1;
}

function foo() {
  let o1 = new MyObj();
  let o2 = new MyObj();
  let o3 = new MyObj();
  let o4 = new MyObj();
  let o5 = new MyObj();

  // Initial values are all 1.
  // o1.x=1, o2.x=1, o3.x=1, o4.x=1, o5.x=1

  for (let i = 0; i < 42; i++) {
    // Creating a chain of loads/stores: in the first iteration, o1.x, o2.x,
    // o3.x and o4.x will stay the same but o5.x will be changed, then on the
    // next iteration this will lead o4.x to be changed as well, which on the
    // next iteration will cause o3.x to be changed as well, etc all the way to
    // o1.x.

    o1.x = o2.x;
    o2.x = o3.x;
    o3.x = o4.x;
    o4.x = o5.x;

    if (i != 5) {
      o5.x = 100;
    } else {
      o5.x = 200;
    }
  }

  %DeoptimizeNow();
  return o1.x;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(foo);
assertEquals(100, foo());
assertEquals(100, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(100, foo());
