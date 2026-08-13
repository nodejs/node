// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev --turbolev-escape-analysis

function MyObj() {
  this.x = 17;
  this.y = 19;
}
function MyObj2() {
  this.a = 1;
}
function Outer() {
  this.field = 0;
}

function foo() {
  let o = new MyObj();
  let o2 = new MyObj2();
  let o1 = new Outer();

  // Record dependency o -> o2 before loop
  o.x = o2;

  for (let i = 0; i < 42; i++) {
    // o1.field stores o2.a (which will have a loop Phi)
    o1.field = o2.a;

    // o2.a is modified to force loop Phi.
    if (i != 5) {
      o2.a = 100;
    } else {
      o2.a = 200;
    }

    // o escapes here during 2nd pass due to map check.
    if (i != 10) {
      o.y = 23;
    } else {
      o.y = 24;
    }

    if (i == 17) {
      // Transitioning to cause map change on backedge.
      o.z = 37;
    }
  }

  // Deopt to reconstruct o1.
  %DeoptimizeNow();

  return o1.field;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(MyObj2);
%PrepareFunctionForOptimization(Outer);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
assertEquals(100, foo());
