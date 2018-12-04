// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = 42;

function g(n) {
  while (n > 0) {
    a = new Array(n);
    n--;
  }
}

g(1);

function f() {
  g();
}

f();
%OptimizeFunctionOnNextCall(f);
f();
assertEquals(1, a.length);
