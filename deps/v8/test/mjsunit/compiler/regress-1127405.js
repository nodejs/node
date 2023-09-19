// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --no-analyze-environment-liveness --no-use-ic --assert-types

const symbol = Symbol();

function foo(x) {
  try { x[symbol] = 42 } catch (e) {}
  new Number();
}

%PrepareFunctionForOptimization(foo);
foo({});
%OptimizeFunctionOnNextCall(foo);
foo({});
