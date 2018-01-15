// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var x = { a: 1 }
  var set = new Set();
  set.add(x);

  x.b = 1;
  x.c = 2;
  x.d = 3;
  x.e = 4;
  x.f = 5;
  x.g = 6;

  assertTrue(set.has(x));
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
