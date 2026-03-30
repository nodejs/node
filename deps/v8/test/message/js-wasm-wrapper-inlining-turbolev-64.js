// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev-inline-js-wasm-wrappers --turboshaft-wasm-in-js-inlining
// Flags: --turbolev --no-maglev
// Flags: --trace-turbo-inlining --trace-deopt
// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

// Wrapper inlining with i64 arguments is not supported on 32-bit architectures.
(function testJsWasmWrapperInliningWithI64Args() {
  print("testJsWasmWrapperInliningWithI64Args");
  var builder = new WasmModuleBuilder();

  builder.addFunction("squareI64", kSig_l_l)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI64Mul])
    .exportAs("squareI64");

  let instance = builder.instantiate({});

  function callSquare(n) {
    return instance.exports.squareI64(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  callSquare(1n);
  print("Test BigInt can be converted");
  %OptimizeFunctionOnNextCall(callSquare);
  callSquare(3n);

  // Invalid arguments throw an exception.
  print("Test input of unconvertible type");
  assertThrows(
    () => callSquare({ x: 4 }), SyntaxError);
  assertThrows(
    () => callSquare("test"), SyntaxError);
})();
