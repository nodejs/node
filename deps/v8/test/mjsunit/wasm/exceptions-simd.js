// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --experimental-wasm-simd --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");
load("test/mjsunit/wasm/exceptions-utils.js");

(function TestThrowS128Default() {
  var builder = new WasmModuleBuilder();
  var kSig_v_s = makeSig([kWasmS128], []);
  var except = builder.addException(kSig_v_s);
  builder.addFunction("throw_simd", kSig_v_v)
      .addLocals({s128_count: 1})
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, 0,
      ])
      .exportFunc();
  var instance = builder.instantiate();

  assertWasmThrows(instance, except, [0, 0, 0, 0, 0, 0, 0, 0],
                   () => instance.exports.throw_simd());
})();

(function TestThrowCatchS128Default() {
  var builder = new WasmModuleBuilder();
  var kSig_v_s = makeSig([kWasmS128], []);
  var except = builder.addException(kSig_v_s);
  builder.addFunction("throw_catch_simd", kSig_i_v)
      .addLocals({s128_count: 1})
      .addBody([
        kExprTry, kWasmS128,
          kExprGetLocal, 0,
          kExprThrow, 0,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
        // TODO(mstarzinger): Actually return some compressed form of the s128
        // value here to make sure it is extracted properly from the exception.
        kExprDrop,
        kExprI32Const, 1,
      ])
      .exportFunc();
  var instance = builder.instantiate();

  assertEquals(1, instance.exports.throw_catch_simd());
})();
