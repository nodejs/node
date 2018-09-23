// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var a = new Array(100000);
  var i = 0;
  while (!%HasFastDoubleElements(a)) {
    a[i] = i;
    i += 0.1;
  }
  a[1] = 1.5;
}

f();
%OptimizeFunctionOnNextCall(f);
f();
