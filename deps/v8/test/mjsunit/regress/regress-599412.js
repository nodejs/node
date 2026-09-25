// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function h(a) {
  if (!a) return false;
  print();
}

function g(a) {
  return a.length;
}
g('0');
g('1');

function f() {
  h(g([]));
};
%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
