// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a) {
  var x;
  a = a|0;
  var dummy;
  if (a === 1) {
    x = 277.5;
  } else if (a === 2) {
    x = 0;
  } else {
    dummy = 527.5;
    dummy = 958.5;
    dummy = 1143.5;
    dummy = 1368.5;
    dummy = 1558.5;
    x = 277.5;
  }
  return +x;
}

f();
f();
%OptimizeFunctionOnNextCall(f);
assertEquals(277.5, f());
