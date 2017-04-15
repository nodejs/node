// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function g(a) {
  if (a) return arguments;
  %DeoptimizeNow();
  return 23;
}
function f() {
  return g(false);
}
assertEquals(23, f());
assertEquals(23, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(23, f());
