// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev
// Flags: --turbolev-escape-analysis

function MyObj() {
  this.x = 17;
}

function Wrapper(val) {
  this.inner = val;
}

function test_phi_load_before_loop(cond, n) {
  let wrapper = new Wrapper(new MyObj());
  let o = 0;
  let i = 0;

  while (i < n) {
    // A loop phi will be created for {o}, whose backedge will be a
    // LoadTaggedField (not eliminated during graph building because we're in
    // a loop) that gets elided by Escape Analysis.
    o = wrapper.inner;
    i++;
  }
  return o.x;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(Wrapper);
%PrepareFunctionForOptimization(test_phi_load_before_loop);
test_phi_load_before_loop(true, 1);
test_phi_load_before_loop(false, 1);

%OptimizeFunctionOnNextCall(test_phi_load_before_loop);
test_phi_load_before_loop(true, 1);
