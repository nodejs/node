// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function h(x) {
  return (x|0) && (1>>x == {})
}

function g() {
  return h(1)
};

function f() {
  return g(h({}))
};

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
