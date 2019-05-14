// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --function-context-specialization

function f() {
  var o = {};
  function g() {
    o.x = 1;
    return Object.create(o);
  };
  gc();
  o.x = 10;
  %OptimizeFunctionOnNextCall(g);
  g();
}
f();
f();
