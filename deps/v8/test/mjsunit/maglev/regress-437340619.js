// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let f = () => new Uint32Array();
function foo(a) {
  let x = a + 18956.586;
  let y = f[1];
  print(y);
  f = x;
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
