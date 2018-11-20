// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var arguments_ = arguments;
  if (undefined) {
    while (true) {
      arguments_[0];
    }
  } else {
    %DeoptimizeNow();
    return arguments_[0];
  }
};

f(0);
f(0);
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f(1));

function g() {
    var a = arguments;
    %DeoptimizeNow();
    return a.length;
}

g(1);
g(1);
%OptimizeFunctionOnNextCall(g);
assertEquals(1, g(1));
