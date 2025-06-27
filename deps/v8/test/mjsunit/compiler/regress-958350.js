// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(o) {
  for (const x of o) {
    o[100] = 1;
    try { x.push(); } catch (e) {}
  }
}

%PrepareFunctionForOptimization(foo);
foo([1]);
foo([1]);
%OptimizeFunctionOnNextCall(foo);
foo([1]);
