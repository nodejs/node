// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function f() {
  const a = {'x': 5};
  a.x = {};
  Object.freeze(a);
  return Object.is(a.x, 5)
}

%PrepareFunctionForOptimization(f);
assertFalse(f());
assertFalse(f());
%OptimizeFunctionOnNextCall(f);
assertFalse(f());
assertOptimized(f);
