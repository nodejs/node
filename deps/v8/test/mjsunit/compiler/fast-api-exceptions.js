// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api

const fast_c_api = new d8.test.FastCAPI();

(function main() {
  let thrown_count = 0;
  function fallback_and_throw() {
    try {
      thrown_count++;
      fast_c_api.throw_fallback();
    } catch (err) {
      thrown_count--;
    }
  }

  %PrepareFunctionForOptimization(fallback_and_throw);
  fallback_and_throw();
  assertEquals(0, thrown_count);

  %OptimizeFunctionOnNextCall(fallback_and_throw);
  fallback_and_throw();
  assertEquals(0, thrown_count);
})();
