// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  if (x === undefined) return;
  while (true) {
    while (1 || 2) { }
    f();
  }
}
%OptimizeFunctionOnNextCall(foo);
foo();
