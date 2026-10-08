// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* inner() {
  yield "inner value 1";
  yield "inner value 2";
  return "inner done";
}

function* myGen() {
  yield "value 1";
  let res = yield* inner();
  yield res;
  return "outer done";
}

function test(g) {
  // Initialize as PACKED_ELEMENTS; this avoids problems with
  // the single_generation config not handling AllocationSites and
  // leading to spurious deopts.
  let res = [""];
  res.pop();
  for (let x of g) res.push(x);
  return res;
}
%PrepareFunctionForOptimization(test);

assertEquals(["value 1", "inner value 1", "inner value 2", "inner done"], test(myGen()));
assertEquals(["value 1", "inner value 1", "inner value 2", "inner done"], test(myGen()));

%OptimizeMaglevOnNextCall(test);
assertEquals(["value 1", "inner value 1", "inner value 2", "inner done"], test(myGen()));
assertOptimized(test);
