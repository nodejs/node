// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
var a = new Array(128);

function f(a, base) {
  a[base] = 2;
}

f(a, undefined);
f("r12", undefined);
f(a, 0);
%OptimizeFunctionOnNextCall(f);
f(a, 0);
