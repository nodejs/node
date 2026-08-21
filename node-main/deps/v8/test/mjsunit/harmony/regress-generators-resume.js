// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function* foo() {
  for (let i = 0; i < 10; i++) {
    yield 1;
  }
  return 0;
}

%PrepareFunctionForOptimization(foo);
g = foo();
%OptimizeFunctionOnNextCall(foo);
g.next();
g.next();
