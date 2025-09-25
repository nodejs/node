// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Test storing JS HeapNumbers (non-Smis) on shared objects. This requires the
// wasm wrapper to properly "migratr" the allocated HeapNumber to the shared
// heap.
(function TestStructSetUnsharedHeapNumber() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let anyRefT = wasmRefNullType(kWasmAnyRef).shared();
  let externRefT = wasmRefNullType(kWasmExternRef).shared();
  let struct = builder.addStruct({
    fields: [
      makeField(anyRefT, true),
      makeField(externRefT, true),
    ],
    is_shared: true,
  });
  let producer_sig = makeSig([anyRefT, externRefT], [wasmRefType(struct)]);
  builder.addFunction("newStruct", producer_sig)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructNew, struct])
    .exportFunc();
  builder.addFunction("atomicSetAny",
      makeSig([wasmRefNullType(struct), anyRefT], []))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0,
    ])
    .exportFunc();
  builder.addFunction("atomicSetAnyConvertExtern",
      makeSig([wasmRefNullType(struct), externRefT], []))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprAnyConvertExtern,
      kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0,
    ])
    .exportFunc();
  builder.addFunction("setExtern",
      makeSig([wasmRefNullType(struct), externRefT], []))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      // At least at the time of writing, there isn't an atomic.set for
      // externref, so we need to use a non-atomic instruction.
      kGCPrefix, kExprStructSet, struct, 1,
    ])
    .exportFunc();

  let wasm = builder.instantiate().exports;
  let struct1 = wasm.newStruct(-1.1, -2.2);
  wasm.atomicSetAny(struct1, 123.456);
  let struct2 = wasm.newStruct(-1.1, -2.2);
  wasm.atomicSetAnyConvertExtern(struct2, 123.456);
  let struct3 = wasm.newStruct(-1.1, -2.2);
  wasm.setExtern(struct3, 123.456);

  // In build-configurations with 32 bit smis, large Smis need to be converted
  // to HeapNumbers when converting to anyref, so that anyref smis are always
  // guaranteed to be i31ref-compatible. For shared anyref, this conversion
  // needs to allocate shared HeapNumbers.
  let smi32 = 1073741824; // 2^30
  let struct4 = wasm.newStruct(-1.1, -2.2);
  wasm.atomicSetAny(struct4, smi32);
  let struct5 = wasm.newStruct(-1.1, -2.2);
  wasm.atomicSetAnyConvertExtern(struct5, smi32);
  let struct6 = wasm.newStruct(-1.1, -2.2);
  wasm.setExtern(struct6, smi32);
  // Trigger gc for optional heap-verification.
  // This will check whether the HeapNumber stored in the shared struct is
  // actually converted to a shared HeapNumber.
  gc();
})();
