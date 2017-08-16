// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  var s = x ? "0" : "1";
  return 1 + Number(s);
}

f(0);
f(0);
%OptimizeFunctionOnNextCall(f);
assertEquals(2, f(0));
