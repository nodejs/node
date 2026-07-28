// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $struct0 = builder.nextTypeIndex();
/* $struct0 */ builder.addStruct({fields: [makeField(wasmRefNullType($struct0), false)], final: true, shared: true});
let $array1 = builder.addArray(wasmRefNullType($struct0), false, kNoSuperType, true);
let $sig2 = builder.addType(makeSig([kWasmI32, kWasmI32], [wasmRefType($array1)]));
let create_shared_array = builder.addFunction(undefined, $sig2).exportAs('create_shared_array');
let $segment0 = builder.addPassiveElementSegment([[kGCPrefix, kExprStructNewDefault, $struct0]], wasmRefNullType($struct0));

create_shared_array.addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kGCPrefix, kExprArrayNewElem, $array1, $segment0,
]);

const instance = builder.instantiate({});
instance.exports.create_shared_array();
