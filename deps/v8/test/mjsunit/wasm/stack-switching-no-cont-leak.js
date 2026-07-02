// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function TestContLeak(indexed) {
  let builder = new WasmModuleBuilder();
  let sig_v_v = builder.addType(kSig_v_v);
  let cont_index = builder.addCont(sig_v_v);
  let cont_type = indexed ? cont_index : kWasmContRef;
  let tag_index = builder.addTag(kSig_v_v);
  let sig_cont = makeSig([], [wasmRefNullType(cont_type)]);
  let sig_tag_cont = makeSig([wasmRefNullType(cont_type)], []);
  let tag_cont_index = builder.addTag(sig_tag_cont);

  let g_cont = builder.addGlobal(wasmRefNullType(cont_type), true, false);
  builder.addExportOfKind("g_cont", kExternalGlobal, g_cont.index);

  let table = builder.addTable(wasmRefNullType(cont_type), 1, 1).exportAs("table");

  let suspend = builder.addFunction("suspend", kSig_v_v).addBody([
      kExprSuspend, tag_index
  ]).exportFunc();

  builder.addFunction("get_cont", sig_cont)
    .addBody([kExprGlobalGet, g_cont.index])
    .exportFunc();

  builder.addFunction("throw_cont", kSig_v_v)
    .addBody([kExprGlobalGet, g_cont.index, kExprThrow, tag_cont_index])
    .exportFunc();

  builder.addExportOfKind("tag_cont", kExternalTag, tag_cont_index);

  builder.addFunction("main", kSig_v_v).addBody([
      kExprBlock, kWasmRef, cont_index,
        kExprRefFunc, suspend.index,
        kExprContNew, cont_index,
        kExprResume, cont_index, 1,
          kOnSuspend, tag_index, 0,
        kExprUnreachable,
      kExprEnd,
      kExprGlobalSet, g_cont.index,
      kExprI32Const, 0,
      kExprGlobalGet, g_cont.index,
      kExprTableSet, table.index,
  ]).exportFunc();

  let instance = builder.instantiate();
  instance.exports.main();

  // 1. Global leak.
  assertThrows(
      () => instance.exports.g_cont.value,
      TypeError, /invalid type/);

  // 2. Table leak.
  assertThrows(
      () => instance.exports.table.get(0),
      TypeError, /invalid type/);

  // 3. Function return leak.
  assertThrows(
      () => instance.exports.get_cont(),
      TypeError, /type incompatibility/);

  // 4. Exception leak.
  assertThrows(() => {
      try {
          instance.exports.throw_cont();
      } catch (e) {
          e.getArg(instance.exports.tag_cont, 0);
      }
  }, TypeError, /invalid type/);
}

// Test with abstract cont type.
TestContLeak(false);
// Test with indexed cont type.
TestContLeak(true);
