// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var a = {b: -1.5};
  for (var i = 0; i < 1; i++) {
    a.b = 1;
  }
  assertTrue(0 <= a.b);
}

foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
