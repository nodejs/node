// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var x = 0;
x = 42;

function foo(a, b) {
  let y = a < a;
  if (b) x = y;
}

foo(1, false);
foo(1, false);
%OptimizeFunctionOnNextCall(foo);
foo(1, true);
