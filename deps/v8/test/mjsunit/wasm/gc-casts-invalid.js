// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestRefTestInvalid() {
  let struct = 0;
  let array = 1;
  let sig = 2;
  let types = [
    // source value type |target heap type
    [kWasmI32,            kAnyRefCode],
    [kWasmNullExternRef,  struct],
    [wasmRefType(struct), kNullFuncRefCode],
    [wasmRefType(array),  kFuncRefCode],
    [wasmRefType(struct), sig],
    [wasmRefType(sig),    struct],
    [wasmRefType(sig),    kExternRefCode],
    [kWasmAnyRef,         kExternRefCode],
    [kWasmAnyRef,         kFuncRefCode],
  ];
  let casts = [
    kExprRefTest,
    kExprRefTestNull,
  ];

  for (let [source_type, target_type_imm] of types) {
    for (let cast of casts) {
      let builder = new WasmModuleBuilder();
      assertEquals(struct, builder.addStruct([makeField(kWasmI32, true)]));
      assertEquals(array, builder.addArray(kWasmI32));
      assertEquals(sig, builder.addType(makeSig([kWasmI32], [])));
      builder.addFunction('refTest', makeSig([kWasmI32], [source_type]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, cast, target_type_imm,
      ]);

      assertThrows(() => builder.instantiate(),
                  WebAssembly.CompileError,
                  /has to be in the same reference type hierarchy/);
    }
  }
})();
