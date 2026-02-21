// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(x) {
  x.f = 13.37;
}

function foo() {
  const v2 = {};
  const v3 = {a:42};
  const v4 = {a:42};
  v3.d = 42;
  v4.b = v2;
  v4.b = 42;
  v4.b;
  v3.f = v2;
  bar(v4);
  const v10 = {a:42};
  for (let i = 0; i < 10; i++) {
    bar(v10);
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
