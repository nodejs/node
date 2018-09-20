// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

var foo = { x : 0, y : 1 }

function f(b) {
  h(b);
  return foo.x;
}

function h(b) {
  g(b);
  return foo.x;
}

function g(b) {
  if (b) {
    foo = { x : 536 };
  } // It should trigger an eager deoptimization when b=true.
}

f(false); f(false);
%OptimizeFunctionOnNextCall(f);
f(false);
assertEquals(f(true), 536);
