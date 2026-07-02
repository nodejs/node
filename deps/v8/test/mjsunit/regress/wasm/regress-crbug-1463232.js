// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function CastToView() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_v_r)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCastNull, kStringViewWtf16Code,
      kExprDrop,
    ]).exportFunc();

  assertThrows(() => builder.instantiate().exports.main("foo"),
               WebAssembly.CompileError,
               /has to be in the same reference type hierarchy/);
})();

(function TestView() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_v_r)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefTestNull, kStringViewWtf16Code,
      kExprDrop,
    ]).exportFunc();

    assertThrows(() => builder.instantiate().exports.main("foo"),
                 WebAssembly.CompileError,
                 /has to be in the same reference type hierarchy/);
})();

(function TestBranchOnCast() {
  let builder = new WasmModuleBuilder();

  const view = kStringViewWtf16Code;

  builder.addFunction("main", kSig_v_r)
    .addBody([
      kExprBlock, kWasmVoid,
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprBrOnCast, 0b11, 0, kAnyRefCode, view,
      kExprDrop,
      kExprEnd,
    ]).exportFunc();

    assertThrows(() => builder.instantiate().exports.main("foo"),
                 WebAssembly.CompileError,
                 /nullable string views don't exist/);
})();

(function TestBranchOnCastFail() {
  let builder = new WasmModuleBuilder();

  const view = kStringViewWtf16Code;

  builder.addFunction("main", kSig_v_r)
    .addBody([
      kExprBlock, kAnyRefCode,
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprBrOnCastFail, 0b11, 0, kAnyRefCode, view,
      kExprDrop,
      kExprRefNull, kAnyRefCode,
      kExprEnd,
      kExprDrop,
    ]).exportFunc();

    assertThrows(() => builder.instantiate().exports.main("foo"),
                 WebAssembly.CompileError,
                 /nullable string views don't exist/);
})();
