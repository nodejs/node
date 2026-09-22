// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev
// Flags: --turbolev-escape-analysis

function Wrapper(val) {
  this.f = val;
}

function test() {
  const wrapper = new Wrapper(0);

  for (let i = 0; i < 2; i++) {

    for (let j = 0; j < 2; j++) {
      let cur = wrapper.f;
      wrapper.f = 100;
    }

    wrapper.f = 42;
  }
}

%PrepareFunctionForOptimization(Wrapper);
%PrepareFunctionForOptimization(test);
test();

%OptimizeFunctionOnNextCall(test);
test();
