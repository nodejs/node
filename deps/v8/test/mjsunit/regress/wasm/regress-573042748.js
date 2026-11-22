// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-js-interop

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let $array_externref = builder.addArray(kWasmExternRef, {final: true});
let $array_funcref = builder.addArray(kWasmFuncRef, {final: true});
let $array_i8 = builder.addArray(kWasmI8, {final: true});
let $configureAll = builder.addImport(
    'wasm:js-prototypes', 'configureAll',
    makeSig(
        [
          wasmRefNullType($array_externref),
          wasmRefNullType($array_funcref),
          wasmRefNullType($array_i8),
          kWasmExternRef,
        ],
        []));
let $constructors =
    builder.addImportedGlobal('c', 'constructors', kWasmExternRef, false);
let $g_proto = builder.addImportedGlobal('p', 'proto', kWasmExternRef, false);
let $m = builder.addFunction('m', kSig_v_v).addBody([]);

// 1 config, no constructor, 0 statics, 1 method ('m' -> $m), no parent (-1).
let data = [
  1,          // 1 config
  0,          // no constructor / 0 statics
  1,          // 1 method
  0, 1, 109,  // kWasmMethod, name="m"
  0x7f,       // parent = -1
];

let proto_segment = builder.addPassiveElementSegment(
    [[kExprGlobalGet, $g_proto]], kWasmExternRef);
let func_segment = builder.addPassiveElementSegment([$m.index]);
let data_segment = builder.addPassiveDataSegment(data);

// Call configureAll inside a Wasm try_table (catch_all) and return 1 if caught,
// 0 if configureAll did not throw.
builder.addFunction('test', kSig_i_v)
    .addBody([
      kExprBlock, kWasmVoid,
        kExprTryTable, kWasmVoid, 1,
        kCatchAllNoRef, 0,
          kExprI32Const, 0,
          kExprI32Const, 1,
          kGCPrefix, kExprArrayNewElem, $array_externref, proto_segment,

          kExprI32Const, 0,
          kExprI32Const, 1,
          kGCPrefix, kExprArrayNewElem, $array_funcref, func_segment,

          kExprI32Const, 0,
          kExprI32Const, data.length,
          kGCPrefix, kExprArrayNewData, $array_i8, data_segment,
          kExprGlobalGet, $constructors,
          kExprCallFunction, $configureAll,
          kExprI32Const, 0,
          kExprReturn,
        kExprEnd,
      kExprEnd,
      kExprI32Const, 1,
    ])
    .exportFunc();

// Invalid prototype (42 is not an Object or null) causes configureAll to throw
// a catchable TypeError.
let instance = builder.instantiate(
    {c: {constructors: {}}, p: {proto: 42}}, {builtins: ['js-prototypes']});

assertEquals(1, instance.exports.test());
