// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let a = [1,2];
function f(skip) { g(undefined, skip) }
function g(x, skip) {
  if (skip) return;
  return a[x+1];
}
g(0, false);
g(0, false);
f(true);
f(true);
%OptimizeFunctionOnNextCall(f);
f(false);
