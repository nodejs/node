// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-struct --allow-natives-syntax --no-lazy-feedback-allocation

function f() {
  return Atomics.Mutex.prototype;
}

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
assertEquals(null, f());
