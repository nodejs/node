// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan --turbo-inline-js-wasm-calls

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder
    .addFunction("f", kSig_v_l) // i64 -> ()
    .addBody([])
    .exportFunc();

let module = builder.instantiate();

function TestBigIntTruncatedToWord64(x) {
  return module.exports.f(x + x);
}

let bi = (2n ** (2n ** 29n + 2n ** 29n - 1n));

// Expect BigIntTooBig for adding bi to itself
assertThrows(() => TestBigIntTruncatedToWord64(bi), RangeError);

%PrepareFunctionForOptimization(TestBigIntTruncatedToWord64);
TestBigIntTruncatedToWord64(1n);
TestBigIntTruncatedToWord64(2n);
%OptimizeFunctionOnNextCall(TestBigIntTruncatedToWord64);

// After optimization, bi should be checked as BigInt and
// truncated to Word64, which is then passed to Int64Add.
// Thus no BigIntTooBig exception is expected.
TestBigIntTruncatedToWord64(bi);
assertOptimized(TestBigIntTruncatedToWord64);
