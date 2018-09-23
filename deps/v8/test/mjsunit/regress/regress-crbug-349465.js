// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --debug-code --use-gvn

function f(a, base) {
  a[base] = 1;
  a[base + 4] = 2;
  a[base] = 3;
}
var a1 = new Array(1024);
var a2 = new Array(128);
f(a1, 1);
f(a2, -2);
%OptimizeFunctionOnNextCall(f);
f(a1, -2);
