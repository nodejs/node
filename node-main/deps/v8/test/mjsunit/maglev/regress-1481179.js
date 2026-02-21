// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-lazy-feedback-allocation

function f(e, i, a) {
  a[65535] = 42;
  a.e = a;
}
function foo() {
  const a = [1,2];
  a.e = 42;
  a.forEach(f);
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
