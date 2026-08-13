// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev
// Flags: --turbolev-escape-analysis

function MyObj() {
  this.x = 17
  this.y = 19;
}

function foo(b) {
  let o = new MyObj();

  for (let i = 0; i < 42; i++) {
    if (i != 10) {
      // Will have both maps in its feedback. This needs to be before the
      // `o.y=23` below so that it actually records the new value of `o.x`
      // before {o} gets marked as escaped (since once {o} is marked as escaped,
      // we store recording the values of its fields).
      o.x = 19;
    } else {
      // Will only have the initial map in its feedback. When revisiting the
      // loop, this will cause {o} to be marked as escaped because we can't
      // guarantee that it will pass the map check.
      o.y = 23;
    }
    // A phi for `o.x` will be inserted here.

    if (i == 17) {
      // Conditionally transitioning.
      o.z = 37;
    }

  }

  return o.x;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(foo);
foo(true);
foo(true);

%OptimizeFunctionOnNextCall(foo);
foo(true);
