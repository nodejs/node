// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --experimental-wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestContNewExact() {
  const builder = new WasmModuleBuilder();
  const t_i_v = builder.addType(kSig_i_v);
  const c_i_v = builder.addCont(t_i_v);

  const f = builder.addFunction("f", t_i_v)
    .addBody([kExprI32Const, 42]);

  builder.addDeclarativeElementSegment([f.index]);

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprLoop, kWasmRef, kWasmExact, c_i_v,
      kExprRefFunc, f.index,
      kExprContNew, c_i_v,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 42
    ]).exportFunc();

  const instance = builder.instantiate();
  assertEquals(42, instance.exports.main());
})();

(function TestContBindExact() {
  const builder = new WasmModuleBuilder();
  const t_v_i = builder.addType(kSig_v_i);
  const t_v_v = builder.addType(kSig_v_v);
  const c_v_i = builder.addCont(t_v_i);
  const c_v_v = builder.addCont(t_v_v);

  const f = builder.addFunction("f", t_v_i)
    .addBody([]);

  builder.addDeclarativeElementSegment([f.index]);

  builder.addFunction("main", kSig_v_v)
    .addBody([
      kExprLoop, kWasmRef, kWasmExact, c_v_v,
      kExprI32Const, 0,
      kExprRefFunc, f.index,
      kExprContNew, c_v_i,
      kExprContBind, c_v_i, c_v_v,
      kExprEnd,
      kExprDrop
    ]).exportFunc();

  const instance = builder.instantiate();
  instance.exports.main();
})();

(function TestResumeExact() {
  const builder = new WasmModuleBuilder();
  const t_v_v = builder.addType(kSig_v_v);
  const c_v_v = builder.addCont(t_v_v);
  const tag = builder.addTag(kSig_v_v);

  const f = builder.addFunction("f", t_v_v)
    .addBody([kExprSuspend, tag]);
  builder.addDeclarativeElementSegment([f.index]);

  builder.addFunction("main", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, kWasmExact, c_v_v,
          kExprRefFunc, f.index,
          kExprContNew, c_v_v,
          kExprResume, c_v_v, 1,
          kOnSuspend, tag, 0,
          // If f returns normally (not in this test, but needed for validation):
          kExprRefFunc, f.index,
          kExprContNew, c_v_v,
        kExprEnd,
        kExprDrop,
    ]).exportFunc();

  const instance = builder.instantiate();
  instance.exports.main();
})();

(function TestNegativeExactness() {
  const builder = new WasmModuleBuilder();
  const t0 = builder.addType(kSig_v_v);
  const c0 = builder.addCont(t0);

  builder.addFunction("main", makeSig([wasmRefType(c0)], []))
    .addBody([
      kExprLoop, kWasmRef, kWasmExact, c0,
      kExprLocalGet, 0, // (ref c0) is NOT (ref exact c0)
      kExprEnd,
      kExprDrop
    ]);

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError, /type error/);
})();
