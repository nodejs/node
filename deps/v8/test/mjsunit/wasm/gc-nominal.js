// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestNominalTypesBasic() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  builder.setNominal();
  let struct1 = builder.addStruct([makeField(kWasmI32, true)]);
  let struct2 = builder.addStruct(
      [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct1);

  let array1 = builder.addArray(kWasmI32, true);
  let array2 = builder.addArray(kWasmI32, true, array1);

  builder.addFunction("main", kSig_v_v)
      .addLocals(wasmOptRefType(struct1), 1)
      .addLocals(wasmOptRefType(array1), 1)
      .addBody([
        // Check that we can create a struct with explicit RTT...
        kGCPrefix, kExprRttCanon, struct2, kGCPrefix,
        kExprStructNewDefaultWithRtt, struct2,
        // ...and upcast it.
        kExprLocalSet, 0,
        // Check that we can create a struct with implicit RTT.
        kGCPrefix, kExprStructNewDefault, struct2, kExprLocalSet, 0,
        // Check that we can create an array with explicit RTT...
        kExprI32Const, 10,  // length
        kGCPrefix, kExprRttCanon, array2,
        kGCPrefix, kExprArrayNewDefaultWithRtt, array2,
        // ...and upcast it.
        kExprLocalSet, 1,
        // Check that we can create an array with implicit RTT.
        kExprI32Const, 10,  // length
        kGCPrefix, kExprArrayNewDefault, array2, kExprLocalSet, 1])
      .exportFunc();

  // This test is only interested in type checking.
  builder.instantiate();
})();

(function TestSubtypingDepthTooLarge() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.setNominal();
  builder.addStruct([]);
  for (let i = 0; i < 32; i++) builder.addStruct([], i);
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      /subtyping depth is greater than allowed/);
})();

(function TestArrayInitFromDataStatic() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.setNominal();
  let array_type_index = builder.addArray(kWasmI16, true);

  let dummy_byte = 0xff;
  let element_0 = 1000;
  let element_1 = -2222;

  let data_segment = builder.addPassiveDataSegment(
    [dummy_byte, element_0 & 0xff, (element_0 >> 8) & 0xff,
     element_1 & 0xff, (element_1 >> 8) & 0xff]);

  let global = builder.addGlobal(
    wasmRefType(array_type_index), true,
    WasmInitExpr.ArrayInitFromDataStatic(
      array_type_index, data_segment,
      [WasmInitExpr.I32Const(1), WasmInitExpr.I32Const(2)], builder));

  builder.addFunction("global_get", kSig_i_i)
    .addBody([
      kExprGlobalGet, global.index,
      kExprLocalGet, 0,
      kGCPrefix, kExprArrayGetS, array_type_index])
    .exportFunc();

  // parameters: (segment offset, array length, array index)
  builder.addFunction("init_from_data", kSig_i_iii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprArrayInitFromDataStatic,
      array_type_index, data_segment,
      kExprLocalGet, 2,
      kGCPrefix, kExprArrayGetS, array_type_index])
    .exportFunc();

  builder.addFunction("drop_segment", kSig_v_v)
    .addBody([kNumericPrefix, kExprDataDrop, data_segment])
    .exportFunc();

  let instance = builder.instantiate();

  assertEquals(element_0, instance.exports.global_get(0));
  assertEquals(element_1, instance.exports.global_get(1));

  let init = instance.exports.init_from_data;

  assertEquals(element_0, init(1, 2, 0));
  assertEquals(element_1, init(1, 2, 1));

  assertTraps(kTrapArrayTooLarge, () => init(1, 1000000000, 0));
  assertTraps(kTrapDataSegmentOutOfBounds, () => init(2, 2, 0));

  instance.exports.drop_segment();

  assertTraps(kTrapDataSegmentOutOfBounds, () => init(1, 2, 0));
})();

(function TestArrayInitFromData() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.setNominal();
  let array_type_index = builder.addArray(kWasmI16, true);

  let dummy_byte = 0xff;
  let element_0 = 1000;
  let element_1 = -2222;

  let data_segment = builder.addPassiveDataSegment(
    [dummy_byte, element_0 & 0xff, (element_0 >> 8) & 0xff,
     element_1 & 0xff, (element_1 >> 8) & 0xff]);

  let global = builder.addGlobal(
    wasmRefType(array_type_index), true,
    WasmInitExpr.ArrayInitFromData(
      array_type_index, data_segment,
      [WasmInitExpr.I32Const(1), WasmInitExpr.I32Const(2),
       WasmInitExpr.RttCanon(array_type_index)],
      builder));

  builder.addFunction("global_get", kSig_i_i)
    .addBody([
      kExprGlobalGet, global.index,
      kExprLocalGet, 0,
      kGCPrefix, kExprArrayGetS, array_type_index])
    .exportFunc();

  // parameters: (segment offset, array length, array index)
  builder.addFunction("init_from_data", kSig_i_iii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprRttCanon, array_type_index,
      kGCPrefix, kExprArrayInitFromData, array_type_index, data_segment,
      kExprLocalGet, 2,
      kGCPrefix, kExprArrayGetS, array_type_index])
    .exportFunc();

  builder.addFunction("drop_segment", kSig_v_v)
    .addBody([kNumericPrefix, kExprDataDrop, data_segment])
    .exportFunc();

  let instance = builder.instantiate();

  assertEquals(element_0, instance.exports.global_get(0));
  assertEquals(element_1, instance.exports.global_get(1));

  let init = instance.exports.init_from_data;

  assertEquals(element_0, init(1, 2, 0));
  assertEquals(element_1, init(1, 2, 1));

  assertTraps(kTrapArrayTooLarge, () => init(1, 1000000000, 0));
  assertTraps(kTrapDataSegmentOutOfBounds, () => init(2, 2, 0));

  instance.exports.drop_segment();

  assertTraps(kTrapDataSegmentOutOfBounds, () => init(1, 2, 0));
})();
