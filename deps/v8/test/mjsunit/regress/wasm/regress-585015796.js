// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-stringref --wasm-imported-strings-utf8

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestStringNewWtf8ArrayInterveningSideEffects() {
  let builder = new WasmModuleBuilder();
  let array_i8 = builder.addArray(kWasmI8);
  let data0 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"
  let data1 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"
  let data2 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"

  builder.addFunction('drop_before_new_wtf8_liftoff',
                      makeSig([], [kWasmStringRef]))
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data0,
      kNumericPrefix, kExprDataDrop, data0,
      kExprI32Const, 0,
      kExprI32Const, 3,
      ...GCInstr(kExprStringNewWtf8Array),
    ]);

  builder.addFunction('drop_before_new_wtf8_tierup',
                      makeSig([], [kWasmStringRef]))
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data1,
      kNumericPrefix, kExprDataDrop, data1,
      kExprI32Const, 0,
      kExprI32Const, 3,
      ...GCInstr(kExprStringNewWtf8Array),
    ]);

  builder.addFunction('mutate_before_new_wtf8', makeSig([], [kWasmStringRef]))
    .addLocals(wasmRefNullType(array_i8), 1)
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data2,
      kExprLocalTee, 0,
      kExprI32Const, 0,
      ...wasmI32Const(88),  // 'X'
      kGCPrefix, kExprArraySet, array_i8,
      kExprLocalGet, 0,
      kExprI32Const, 0,
      kExprI32Const, 3,
      ...GCInstr(kExprStringNewWtf8Array),
    ]);

  builder.addFunction('use_array_after_new_wtf8',
                      makeSig([], [kWasmStringRef, kWasmI32]))
    .addLocals(wasmRefNullType(array_i8), 1)
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data2,
      kExprLocalTee, 0,
      kExprI32Const, 0,
      kExprI32Const, 3,
      ...GCInstr(kExprStringNewWtf8Array),
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kGCPrefix, kExprArrayGetU, array_i8,
    ]);

  let inlinee = builder.addFunction(
      'inlinee_overwriting_param',
      makeSig([wasmRefNullType(array_i8)], [kWasmStringRef]))
    .addBody([
      kExprLocalGet, 0,
      kExprRefNull, array_i8,
      kExprLocalSet, 0,
      kExprI32Const, 0,
      kExprI32Const, 3,
      ...GCInstr(kExprStringNewWtf8Array),
    ]);

  builder.addFunction('caller_retains_array_across_inlinee',
                      makeSig([], [kWasmStringRef, kWasmI32]))
    .addLocals(wasmRefNullType(array_i8), 1)
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data2,
      kExprLocalTee, 0,
      kExprCallFunction, inlinee.index,
      kExprLocalGet, 0,
      kExprI32Const, 2,
      kGCPrefix, kExprArrayGetU, array_i8,
    ]);

  let instance = builder.instantiate();
  assertEquals('abc', instance.exports.drop_before_new_wtf8_liftoff());
  assertEquals('Xbc', instance.exports.mutate_before_new_wtf8());
  assertEquals(['abc', 98], instance.exports.use_array_after_new_wtf8());
  assertEquals(
      ['abc', 99], instance.exports.caller_retains_array_across_inlinee());

  %WasmTierUpFunction(instance.exports.mutate_before_new_wtf8);
  assertEquals('Xbc', instance.exports.mutate_before_new_wtf8());

  %WasmTierUpFunction(instance.exports.use_array_after_new_wtf8);
  assertEquals(['abc', 98], instance.exports.use_array_after_new_wtf8());

  %WasmTierUpFunction(instance.exports.caller_retains_array_across_inlinee);
  assertEquals(
      ['abc', 99], instance.exports.caller_retains_array_across_inlinee());

  %WasmTierUpFunction(instance.exports.drop_before_new_wtf8_tierup);
  assertEquals('abc', instance.exports.drop_before_new_wtf8_tierup());
})();

(function TestDecodeStringFromUtf8ArrayInterveningSideEffects() {
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  let array_i8 = builder.addArray(kWasmI8, {final: true});
  builder.endRecGroup();

  let decode_utf8 = builder.addImport(
      'wasm:text-decoder', 'decodeStringFromUTF8Array',
      makeSig([wasmRefNullType(array_i8), kWasmI32, kWasmI32],
              [wasmRefType(kWasmExternRef)]));

  let data0 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"
  let data1 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"
  let data2 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"

  builder.addFunction('drop_before_decode_liftoff',
                      makeSig([], [wasmRefType(kWasmExternRef)]))
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data0,
      kNumericPrefix, kExprDataDrop, data0,
      kExprI32Const, 0,
      kExprI32Const, 3,
      kExprCallFunction, decode_utf8,
    ]);

  builder.addFunction('drop_before_decode_tierup',
                      makeSig([], [wasmRefType(kWasmExternRef)]))
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data1,
      kNumericPrefix, kExprDataDrop, data1,
      kExprI32Const, 0,
      kExprI32Const, 3,
      kExprCallFunction, decode_utf8,
    ]);

  builder.addFunction('mutate_before_decode',
                      makeSig([], [wasmRefType(kWasmExternRef)]))
    .addLocals(wasmRefNullType(array_i8), 1)
    .exportFunc()
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 3,
      kGCPrefix, kExprArrayNewData, array_i8, data2,
      kExprLocalTee, 0,
      kExprI32Const, 0,
      ...wasmI32Const(88),  // 'X'
      kGCPrefix, kExprArraySet, array_i8,
      kExprLocalGet, 0,
      kExprI32Const, 0,
      kExprI32Const, 3,
      kExprCallFunction, decode_utf8,
    ]);

  let instance = builder.instantiate({}, {builtins: ['text-decoder']});
  assertEquals('abc', instance.exports.drop_before_decode_liftoff());
  assertEquals('Xbc', instance.exports.mutate_before_decode());

  %WasmTierUpFunction(instance.exports.mutate_before_decode);
  assertEquals('Xbc', instance.exports.mutate_before_decode());

  %WasmTierUpFunction(instance.exports.drop_before_decode_tierup);
  assertEquals('abc', instance.exports.drop_before_decode_tierup());
})();

(function TestShortcutBoundsAndTraps() {
  let builder = new WasmModuleBuilder();
  let array_i8 = builder.addArray(kWasmI8);
  let data0 = builder.addPassiveDataSegment([97, 98, 99]);  // "abc"

  builder.addFunction('shortcut_traps',
                      makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32],
                              [kWasmStringRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,  // segment offset
      kExprLocalGet, 1,  // segment length
      kGCPrefix, kExprArrayNewData, array_i8, data0,
      kExprLocalGet, 2,  // array start
      kExprLocalGet, 3,  // array end
      ...GCInstr(kExprStringNewWtf8Array),
    ]);

  let instance = builder.instantiate();
  let f = instance.exports.shortcut_traps;

  for (let tierup of [false, true]) {
    if (tierup) %WasmTierUpFunction(f);
    assertEquals('abc', f(0, 3, 0, 3));
    assertEquals('b', f(1, 2, 0, 1));
    // Array length too large (> WasmArray::MaxLength(1)).
    assertTraps(kTrapArrayTooLarge, () => f(0, 0x40000000, 0, 1));
    assertTraps(kTrapArrayTooLarge, () => f(0, 0x3ffffff0, 0, 1));
    // Segment OOB even when string slice [start, end) is within segment length.
    assertTraps(kTrapDataSegmentOutOfBounds, () => f(0, 10, 0, 3));
    // Segment OOB takes precedence over array OOB because array.new_data
    // executes before string.new_wtf8_array.
    assertTraps(kTrapDataSegmentOutOfBounds, () => f(0, 10, 0, 20));
    assertTraps(kTrapDataSegmentOutOfBounds, () => f(0x7fffffff, 0, 1, 1));
    // Segment in bounds, string slice out of bounds of array.
    assertTraps(kTrapArrayOutOfBounds, () => f(0, 2, 0, 3));
    assertTraps(kTrapArrayOutOfBounds, () => f(0, 3, 2, 1));
  }
})();
