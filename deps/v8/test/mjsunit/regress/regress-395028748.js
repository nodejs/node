// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api

const fast_api = new d8.test.FastCAPI();

function f6() {
  const v1 = [73261628, 1073741824, -2147483647];
  v1.length = 0;
  fast_api.add_all_sequence(v1);
  return 0;
}
%PrepareFunctionForOptimization(f6);
f6();
%OptimizeFunctionOnNextCall(f6);
f6();
