// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f(x) {
  var o = {a : 0, b: 0};
  if (x == 0) {
    o.a = 1
  } else {
    if (x <= 1) {
      if (x == 2) {
        o.a = 2;
      } else {
        o.a = 1
      }
      o.a = 2;
    } else {
      if (x == 2) {
        o.a = "x";
      } else {
        o.a = "x";
      }
      o.b = 22;
    }
    o.b = 22;
  }
  return o.a + 1;
}

f(0,0);
f(1,0);
f(2,0);
f(3,0);
f(0,1);
f(1,1);
f(2,1);
f(3,1);
%OptimizeFunctionOnNextCall(f);
assertEquals(f(2), "x1");
