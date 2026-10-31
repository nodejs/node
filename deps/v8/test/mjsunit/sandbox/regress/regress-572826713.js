// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

// Tests that array.{new|init}_{elem|data} are hardened against corrupted
// WasmTypeInfo.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $array0 = builder.addArray(kWasmAnyRef);
let $array1 = builder.addArray(kWasmI8);
let $sig2 = builder.addType(makeSig([], [wasmRefNullType($array1)]));
let $sig3 = builder.addType(makeSig([], [wasmRefNullType($array0)]));
let $sig4 = builder.addType(makeSig([kWasmI32, kWasmI32], [kWasmAnyRef]));
let make_i8 = builder.addFunction(undefined, $sig2).exportAs('make_i8');
let make_any = builder.addFunction(undefined, $sig3).exportAs('make_any');
let test = builder.addFunction(undefined, $sig4).exportAs('test');
let $data0 = builder.addPassiveDataSegment([]);
let $segment0 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment1 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment2 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment3 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment4 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment5 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment6 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment7 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment8 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment9 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment10 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment11 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment12 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment13 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment14 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment15 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment16 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment17 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment18 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment19 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment20 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment21 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment22 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment23 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment24 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment25 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment26 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment27 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment28 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment29 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment30 = builder.addPassiveElementSegment([], kWasmAnyRef);
let $segment31 = builder.addPassiveElementSegment([], kWasmAnyRef);

// func $make_i8: [] -> [wasmRefNullType($array1)]
make_i8.addBody([
  kExprI32Const, 4,
  kGCPrefix, kExprArrayNewDefault, $array1,
]);

// func $make_any: [] -> [wasmRefNullType($array0)]
make_any.addBody([
  kExprI32Const, 4,
  kGCPrefix, kExprArrayNewDefault, $array0,
]);

// func $test: [kWasmI32, kWasmI32] -> [kWasmAnyRef]
test.addBody([
  kExprLocalGet, 0,  // $var0
  kExprLocalGet, 1,  // $var1
  kGCPrefix, kExprArrayNewElem, $array0, $segment31,
]);

const instance = builder.instantiate({});

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const i8_array_addr = Sandbox.getAddressOf(instance.exports.make_i8());
const any_array_addr = Sandbox.getAddressOf(instance.exports.make_any());
const i8_map = memory.getUint32(i8_array_addr, true) - 1;
const any_map = memory.getUint32(any_array_addr, true) - 1;

const kWasmTypeInfoOffset = 20;
memory.setUint32(
    any_map + kWasmTypeInfoOffset,
    memory.getUint32(i8_map + kWasmTypeInfoOffset, true),
    true);

assertTraps(kTrapElementSegmentOutOfBounds,
            () => instance.exports.test(0, 0x01000000));
