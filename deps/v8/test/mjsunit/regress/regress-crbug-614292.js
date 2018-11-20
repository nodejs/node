// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  return [] | 0 && values[0] || false;
}

%OptimizeFunctionOnNextCall(foo);
try {
  foo();
} catch (e) {}
