// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

x = 1;
x = 2;
x = 3;

function f() {
  return x;
};
%PrepareFunctionForOptimization(f);
f();
f();
f();
%OptimizeFunctionOnNextCall(f);
f();

Object.defineProperty(this, 'x', {
  get: function() {
    return 100;
  }
});

assertEquals(100, f());
