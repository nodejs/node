// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc --allow-natives-syntax --predictable --stack-size=300

function* f() {
  try {
    g();
  } catch {}
}

function g() {
  try {
    for (var i of f());
  } catch {
    gc();
  }
}

%PrepareFunctionForOptimization(g);
g();
g();
g();
// Brittle repro: depends on exact placement of OptimizeFunctionOnNextCall and
// --stack-size.
%OptimizeFunctionOnNextCall(g);
g();
