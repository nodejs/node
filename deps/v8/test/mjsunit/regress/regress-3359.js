// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  return 1 >> Boolean.constructor + 1;
}
assertEquals(1, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f());
