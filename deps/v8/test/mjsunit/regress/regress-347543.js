// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --debug-code

function f(a) {
  a[5000000] = 256;
  assertEquals(256, a[5000000]);
}

var v1 = new Array(5000001);
var v2 = new Array(10);
f(v1);
f(v2);
f(v2);
%OptimizeFunctionOnNextCall(f);
f(v2);
f(v1);
