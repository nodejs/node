// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

a = {
  y: 1.5
};
a.y = 1093445778;
b = a.y;
c = {
  y: {}
};

function f() {
  return {y: b};
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
assertEquals(f().y, 1093445778);
