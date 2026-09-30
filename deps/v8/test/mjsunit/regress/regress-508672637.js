// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --expose-fast-api --turbo-fast-api-calls
// Flags: --turbofan

const api = new d8.test.FastCAPI();

assertThrows(() => api.sum_uint64_as_number(-1, 0));

function optimizedUint64Call(a) {
  const x = a + 0;
  return api.sum_uint64_as_number(x, 0);
}

%PrepareFunctionForOptimization(optimizedUint64Call);
for (let i = 0; i < 10000; ++i) {
  optimizedUint64Call(2 ** 40 + (i & 1));
}
%OptimizeFunctionOnNextCall(optimizedUint64Call);
optimizedUint64Call(2 ** 40);

assertThrows(() => optimizedUint64Call(-1));
