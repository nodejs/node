// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function foo(a) {
  a[0];
  a[1] = "";
}

%PrepareFunctionForOptimization(foo);
foo([0,0].map(x => x));
foo([0,0].map(x => x));
%OptimizeFunctionOnNextCall(foo);
foo([0,0].map(x => x));
assertOptimized(foo);
