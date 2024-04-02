// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-analyze-environment-liveness --no-use-ic

function foo(x) {
  var a = [];
  for (var k1 in x) {
    for (var k2 in x) {
      a.k2;
    }
  }
  return a.join();
}

%PrepareFunctionForOptimization(foo);
foo({p: 42});
%OptimizeFunctionOnNextCall(foo);
foo();
