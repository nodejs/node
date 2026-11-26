// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --wasm-custom-descriptors --wasm-merged-descriptors --wasm-stringref
// Flags: --wasm-random-rescheduling

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Subtest 1: StringAsWtf16Op must not be scheduled inside an uninitialized
// struct allocation.
(function TestStringAsWtf16() {
  const builder = new WasmModuleBuilder();
  const $struct = builder.addStruct({
    fields: [
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
    ],
  });
  const $g = builder.addGlobal(wasmRefNullType($struct), true, false);

  const kNumFuncs = 8;
  for (let f = 0; f < kNumFuncs; f++) {
    const padding = [];
    for (let p = 0; p < f; p++) {
      padding.push(kExprLocalGet, 0, kExprDrop);
    }
    builder.addFunction(`f${f}`, makeSig([kWasmStringRef], [kWasmI32]))
      .addLocals(kWasmStringViewWtf16, 1)
      .addBody([
        ...padding,
        kExprLocalGet, 0,
        ...GCInstr(kExprStringAsWtf16),
        kExprLocalSet, 1,
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kAnyRefCode,
        kGCPrefix, kExprStructNew, $struct,
        kExprGlobalSet, $g.index,
        kExprLocalGet, 1,
        kExprI32Const, 0,
        ...GCInstr(kExprStringViewWtf16GetCodeunit),
      ])
      .exportFunc();
  }

  const instance = builder.instantiate();
  const a = 'a'.repeat(20);
  const b = 'b'.repeat(20);
  for (let f = 0; f < kNumFuncs; f++) {
    const fn = instance.exports[`f${f}`];
    fn('a');
    const cons = a + (b + f);
    %SetAllocationTimeout(100000, 2, false);
    assertEquals(97, fn(cons));
    %SetAllocationTimeout(-1, 0, true);
  }
})();

// Subtest 2: ExternConvertAnyOp must not be scheduled inside an uninitialized
// struct allocation when allocating a WasmCustomMapWrapper.
(function TestExternConvertAny() {
  const builder = new WasmModuleBuilder();
  builder.startRecGroup();
  const $desc = builder.nextTypeIndex() + 1;
  const $described = builder.addStruct({
    fields: [makeField(kWasmI32, true)],
    descriptor: $desc,
  });
  builder.addStruct({
    fields: [
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
    ],
    describes: $described,
  });
  builder.endRecGroup();

  const $regular = builder.addStruct({
    fields: [
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
      makeField(kWasmAnyRef, true),
    ],
  });

  const $gDesc = builder.addGlobal(wasmRefNullType($desc), true, false);
  const $gReg = builder.addGlobal(wasmRefNullType($regular), true, false);

  builder.addFunction('setupDesc', kSig_v_v)
    .addBody([
      kExprRefNull, kAnyRefCode,
      kExprRefNull, kAnyRefCode,
      kExprRefNull, kAnyRefCode,
      kExprRefNull, kAnyRefCode,
      kGCPrefix, kExprStructNew, $desc,
      kExprGlobalSet, $gDesc.index,
    ])
    .exportFunc();

  const kNumFuncs = 8;
  for (let f = 0; f < kNumFuncs; f++) {
    const padding = [];
    for (let p = 0; p < f; p++) {
      padding.push(kExprI32Const, p, kExprDrop);
    }
    builder.addFunction(`f${f}`, makeSig([], [kWasmExternRef]))
      .addLocals(kWasmAnyRef, 1)
      .addBody([
        ...padding,
        kExprGlobalGet, $gDesc.index,
        kExprLocalTee, 0,
        kExprDrop,
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kAnyRefCode,
        kGCPrefix, kExprStructNew, $regular,
        kExprGlobalSet, $gReg.index,
        kExprLocalGet, 0,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();
  }

  const instance = builder.instantiate();
  for (let f = 0; f < kNumFuncs; f++) {
    instance.exports.setupDesc();
    %SetAllocationTimeout(100000, 2, false);
    assertNotNull(instance.exports[`f${f}`]());
    %SetAllocationTimeout(-1, 0, true);
  }
})();
