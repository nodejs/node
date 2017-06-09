// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var global = new Object(3);
function f() {
  global[0] = global[0] >>> 15.5;
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
