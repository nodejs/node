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
  let o0 = new Outer();

  o.x = o2; // dependency o -> o2

  for (let i = 0; i < 42; i++) {
    o0.field = o1.field;
    o1.field = o2.a;
    o2.a = i;

    if (i != 10) {
      o.y = 23;
    } else {
      o.y = 24;
    }

    if (i == 17) {
      o.z = 37;
    }
  }

  %DeoptimizeNow();
  return o0.field;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(MyObj2);
%PrepareFunctionForOptimization(Outer);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
assertEquals(39, foo());
