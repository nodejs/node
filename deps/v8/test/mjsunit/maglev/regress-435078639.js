// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-use-ic

function foo() {
  let v1 = 2000000000;
  v1--;
  let v2 = v1 << v1;
  v2--;
  print(v2);
}
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
