// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev
// Flags: --turbolev-escape-analysis

function ConstructorA() {
  this.f = 42;
}

function ConstructorB() {
  this.f = 42;
}

function Wrapper(val) {
  this.f = val;
}

function test_while(cond) {
  const o1 = new ConstructorA();
  const o2 = new ConstructorB();

  const wrapper = new Wrapper(0);

  for (let i = 0; i < 3; i++) {
    let j = 0;
    if (cond) {
      wrapper.f = o1;
    } else {
      wrapper.f = o2;
    }

    while (j < 5) {
      let cur = wrapper.f;
      wrapper.f = 0;
      j++;
    }
  }
}

%PrepareFunctionForOptimization(ConstructorA);
%PrepareFunctionForOptimization(ConstructorB);
%PrepareFunctionForOptimization(Wrapper);
%PrepareFunctionForOptimization(test_while);
test_while(true);
test_while(false);
%OptimizeFunctionOnNextCall(test_while);
test_while(true);
