// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function f() {
  Promise.prototype.then.call()
}


%PrepareFunctionForOptimization(f);
try {
  f();
} catch (e) {}
%OptimizeFunctionOnNextCall(f);
assertThrows(() => f(), TypeError);
