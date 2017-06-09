// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls

function f() {
  "use strict";
  return undefined(0, 0);
}
function g() {
  return f();
}
assertThrows(g, TypeError);
assertThrows(g, TypeError);
%OptimizeFunctionOnNextCall(g);
assertThrows(g, TypeError);
