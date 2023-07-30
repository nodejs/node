// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function aux(a, b) {
  if (a) {
    a >> b;
  }
}

function opt() {
  let p = Promise;
  ++p;
  // {p} can be anything that evaluates to false but is not inlined.
  return aux(p, "number");
}

%PrepareFunctionForOptimization(aux);
aux(1n, 1n);
%OptimizeFunctionOnNextCall(aux);
aux(1n, 1n);

%PrepareFunctionForOptimization(opt);
opt();
%OptimizeFunctionOnNextCall(opt);
opt();
