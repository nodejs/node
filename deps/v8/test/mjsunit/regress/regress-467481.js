// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a1, a2) {
  var v7 = a2[0];
  var v8 = a1[0];
  a2[0] = 0.3;
}
v6 = new Array(1);
v6[0] = "tagged";
f(v6, [1]);
v5 = new Array(1);
v5[0] = 0.1;
f(v5, v5);
v5 = new Array(10);
f(v5, v5);
%OptimizeFunctionOnNextCall(f);
f(v5, v5);
v5[0];
