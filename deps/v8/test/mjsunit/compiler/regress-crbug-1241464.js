// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-fast-api-calls --expose-fast-api

(function() {
  const fast_c_api = new d8.test.FastCAPI();
  const func1 = fast_c_api.fast_call_count;
  assertThrows(() => new func1());
  const func2 = fast_c_api.slow_call_count;
  assertThrows(() => new func2());
  const func3 = fast_c_api.reset_counts;
  assertThrows(() => new func3());
})();
