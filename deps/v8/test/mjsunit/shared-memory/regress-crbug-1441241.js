// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-struct

const sharedCtors = [
  new SharedStructType(['x']), SharedArray, Atomics.Mutex, Atomics.Condition
];

for (let ctor of sharedCtors) {
  (function TestSharedConstructors(C) {
    function f(a) {
      a.stack = "boom";
    }
    %PrepareFunctionForOptimization(f);
    Error.captureStackTrace(f);
    f(C);
  })(ctor);
}
