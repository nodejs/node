// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

builder.startRecGroup();
const struct_idx = builder.nextTypeIndex();
const desc_idx = struct_idx + 1;
builder.addStruct({fields: [makeField(kWasmI32, true)], descriptor: desc_idx});
builder.addStruct({fields: [makeField(kWasmI32, true)], describes: struct_idx});
builder.endRecGroup();

builder.addFunction('make', makeSig([], [wasmRefType(struct_idx)]))
    .exportFunc()
    .addBody([
      kGCPrefix, kExprStructNewDefault, desc_idx,
      kGCPrefix, kExprStructNewDefaultDesc, struct_idx,
    ]);

const wasmStructWithDesc = builder.instantiate().exports.make();

function NewTarget() {}
NewTarget.prototype = wasmStructWithDesc;

assertThrows(() => Reflect.construct(Set, [[1]], NewTarget), TypeError);
assertThrows(() => Reflect.construct(Map, [[[1, 2]]], NewTarget), TypeError);
assertThrows(() => Reflect.construct(WeakSet, [[{}]], NewTarget), TypeError);
assertThrows(
    () => Reflect.construct(WeakMap, [[[{}, 1]]], NewTarget), TypeError);
