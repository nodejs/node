// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --turbo-fast-api-calls

const fastApi = new d8.test.FastCAPI();

function callWithNegativeConstant() {
  return fastApi.sum_uint64_as_number(-1, 0);
}

%PrepareFunctionForOptimization(callWithNegativeConstant);
assertThrows(() => callWithNegativeConstant(), Error);
%OptimizeFunctionOnNextCall(callWithNegativeConstant);
assertThrows(() => callWithNegativeConstant(), Error);
