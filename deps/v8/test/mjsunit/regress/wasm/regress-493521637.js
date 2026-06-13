// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
let builder = new WasmModuleBuilder();
let kSharedRefString = wasmRefType(kWasmStringRef).shared();
let struct = builder.addStruct({fields: [makeField(kSharedRefString, true)],
                                shared: true});
builder.addFunction("put_in_struct",
                    makeSig([kSharedRefString], [wasmRefType(struct)]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
  .exportFunc();

assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
             /shared string type 'shared string' is not supported/);
