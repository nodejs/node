// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape --turbo-experimental

function f() {
  var x = {};
  x.a = "a";
  x.b = "b";
  assertEquals("a", x.a);
  assertEquals("b", x.b);
}
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
