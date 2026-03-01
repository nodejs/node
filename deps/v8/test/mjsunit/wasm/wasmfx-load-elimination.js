// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --no-liftoff
// Flags: --turboshaft-wasm-load-elimination --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestWasmFXLoadElimination() {
  let builder = new WasmModuleBuilder();

  let struct_type = builder.addStruct([makeField(kWasmI32, true)]);
  let sig_v_v = builder.addType(kSig_v_v);
  let sig_v_i = builder.addType(makeSig([kWasmI32], []));
  let cont_index = builder.addCont(sig_v_v);
  let tag_index = builder.addTag(sig_v_i);

  // Result order: [bottom, top] -> [i32, cont]
  let sig_block_index =
      builder.addType(makeSig([], [kWasmI32, wasmRefNullType(cont_index)]));

  let suspender = builder.addFunction('suspender', kSig_v_v)
      .addBody([
          kExprI32Const, 123,
          kExprSuspend, tag_index,
      ]);

  builder.addDeclarativeElementSegment([suspender.index]);

  builder.addFunction('main', kSig_v_v)
      .addLocals(wasmRefType(struct_type), 1)
      .addBody([
          kExprI32Const, 42,
          ...GCInstr(kExprStructNew), struct_type,
          kExprLocalSet, 0,

          kExprBlock, ...wasmSignedLeb(sig_block_index),
              kExprRefFunc, suspender.index,
              kExprContNew, cont_index,
              kExprResume, cont_index, 1,
              kOnSuspend, tag_index, 0,
              kExprUnreachable,
          kExprEnd,

          kExprDrop, // drop cont
          kExprDrop, // drop i32

          kExprLocalGet, 0,
          ...GCInstr(kExprStructGet), struct_type, 0,
          kExprDrop,
      ]).exportFunc();

  let instance = builder.instantiate();

  instance.exports.main();
})();
