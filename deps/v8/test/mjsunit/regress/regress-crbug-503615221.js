// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/503615221. Turboshaft BuildGraph must not
// fail the dominating_frame_state.valid() DCHECK when compiling a function
// that inlines Array.prototype.sort with a comparator that calls a function
// containing a try-catch around always-throwing code.

function thrower() {
  try {
    Proxy(Proxy, Object());
  } catch(e) {}
}

function f() {
  const a = [f, f];
  function cmp(x, y) {
    thrower();
    return y;
  }
  a.sort(cmp);
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
