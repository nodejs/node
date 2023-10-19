// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft
// Flags: --stress-concurrent-inlining  --expose-fast-api

const fast_c_api = new d8.test.FastCAPI();

function foo() {
  fast_c_api.enforce_range_compare_u64(undefined, "", "/0/");
}

%PrepareFunctionForOptimization(foo);
try {
  foo();
} catch {
}

%OptimizeFunctionOnNextCall(foo);
try {
  foo();
} catch {
}
