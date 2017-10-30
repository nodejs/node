// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f(x) {
  var a = [0.1, 0.2, 0.3];
  %_DeoptimizeNow();
  return a.length;
}

assertEquals(3, f());
assertEquals(3, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(3, f());
