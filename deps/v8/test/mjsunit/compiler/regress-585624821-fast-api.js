// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --turbo-fast-api-calls

const fast_c_api = new d8.test.FastCAPI();
fast_c_api[Symbol.hasInstance] = fast_c_api.is_fast_c_api_object;

function check_instanceof(x) {
  return x instanceof fast_c_api;
}

%PrepareFunctionForOptimization(check_instanceof);
assertTrue(check_instanceof(fast_c_api));
assertFalse(check_instanceof({}));

fast_c_api.reset_counts();
%OptimizeFunctionOnNextCall(check_instanceof);
assertTrue(check_instanceof(fast_c_api));
assertEquals(1, fast_c_api.fast_call_count() + fast_c_api.slow_call_count());
