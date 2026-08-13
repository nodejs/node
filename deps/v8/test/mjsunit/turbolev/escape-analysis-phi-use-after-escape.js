// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev --turbolev-escape-analysis

function MyObj() {
  this.x = 17;
}

function Outer() {
  this.field = 0;
}

function foo(cond) {
  let o1 = new Outer();
  let o = new MyObj();

  if (cond) {
    o.x = 19;
  } else {
    o.x = 23;
  }

  // A phi for `o.x` is created here, at the merge.
  // o1 stores this Phi.
  o1.field = o.x;

  // Now o escapes!
  global_o = o;

  // Deoptimize to reconstruct o1
  %DeoptimizeNow();

  return o1.field;
}

let global_o;

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(Outer);
%PrepareFunctionForOptimization(foo);

foo(true);
foo(false);

%OptimizeFunctionOnNextCall(foo);
assertEquals(19, foo(true));
assertEquals(23, foo(false));
