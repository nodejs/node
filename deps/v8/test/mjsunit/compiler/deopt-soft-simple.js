// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax


function change_o() {
  o = { y : 0, x : 1 };
}

var o = { x : 1 };

function f() {
  // It triggers a soft deoptimization.
  change_o();
  return o.x;
}

%OptimizeFunctionOnNextCall(f);
f();
