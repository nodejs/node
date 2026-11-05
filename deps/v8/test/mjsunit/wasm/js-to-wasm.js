// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-merged-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let $struct = builder.addStruct([makeField(kWasmI32, true)]);

builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct_desc = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  descriptor: $desc,
});
builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  describes: $struct_desc,
});
builder.endRecGroup();

let $array = builder.addArray(kWasmI32);
let $sig = builder.addType(kSig_i_i);

// Pass values through a top-type global so the decoder does not fold ref.test
// via TypeCheckAlwaysSucceeds.
let $g_any = builder.addGlobal(kWasmAnyRef, true, false);
let $g_func = builder.addGlobal(kWasmFuncRef, true, false);

let $dummy_func = builder.addFunction('dummy_func', $sig).addBody([
  kExprLocalGet,
  0,
]);
builder.addDeclarativeElementSegment([$dummy_func.index]);

// Factory functions.
builder.addFunction('make_struct', makeSig([], [wasmRefType($struct)]))
    .addBody([kGCPrefix, kExprStructNewDefault, $struct])
    .exportFunc();

builder.addFunction('make_desc', makeSig([], [wasmRefType($desc)]))
    .addBody([kGCPrefix, kExprStructNewDefault, $desc])
    .exportFunc();

builder
    .addFunction('make_struct_desc', makeSig([], [wasmRefType($struct_desc)]))
    .addBody([
      kGCPrefix,
      kExprStructNewDefault,
      $desc,
      kGCPrefix,
      kExprStructNewDefaultDesc,
      $struct_desc,
    ])
    .exportFunc();

builder.addFunction('make_array', makeSig([], [wasmRefType($array)]))
    .addBody([kExprI32Const, 2, kGCPrefix, kExprArrayNewDefault, $array])
    .exportFunc();

builder.addFunction('make_func', makeSig([], [wasmRefType($sig)]))
    .addBody([kExprRefFunc, $dummy_func.index])
    .exportFunc();

const kTypeConfigs = [
  {
    name: 'externref',
    heap_type: kWasmExternRef,
    kind: 'untestable',
    valid: [
      'js_obj', 'smi', 'heapnumber', 'struct', 'struct_desc', 'desc', 'array',
      'func'
    ],
  },
  {
    name: 'anyref',
    heap_type: kWasmAnyRef,
    kind: 'untestable',
    valid: [
      'js_obj', 'smi', 'heapnumber', 'struct', 'struct_desc', 'desc', 'array',
      'func'
    ],
  },
  {
    name: 'eqref',
    heap_type: kWasmEqRef,
    type_code: [kEqRefCode],
    global: $g_any,
    kind: 'ref_test',
    valid: ['smi', 'heapnumber', 'struct', 'struct_desc', 'desc', 'array'],
  },
  {
    name: 'i31ref',
    heap_type: kWasmI31Ref,
    type_code: [kI31RefCode],
    global: $g_any,
    kind: 'ref_test',
    valid: ['smi', 'heapnumber'],
  },
  {
    name: 'structref',
    heap_type: kWasmStructRef,
    type_code: [kStructRefCode],
    global: $g_any,
    kind: 'ref_test',
    valid: ['struct', 'struct_desc', 'desc'],
  },
  {
    name: 'arrayref',
    heap_type: kWasmArrayRef,
    type_code: [kArrayRefCode],
    global: $g_any,
    kind: 'ref_test',
    valid: ['array'],
  },
  {
    name: 'funcref',
    heap_type: kWasmFuncRef,
    type_code: [kFuncRefCode],
    global: $g_func,
    kind: 'ref_test',
    valid: ['func'],
  },
  {
    name: 'none',
    heap_type: kWasmNullRef,
    kind: 'is_null',
    valid: [],
  },
  {
    name: 'nofunc',
    heap_type: kWasmNullFuncRef,
    kind: 'is_null',
    valid: [],
  },
  {
    name: 'noextern',
    heap_type: kWasmNullExternRef,
    kind: 'is_null',
    valid: [],
  },
  {
    name: 'struct',
    heap_type: $struct,
    type_code: wasmSignedLeb($struct),
    global: $g_any,
    kind: 'ref_test',
    valid: ['struct'],
  },
  {
    name: 'struct_desc',
    heap_type: $struct_desc,
    type_code: wasmSignedLeb($struct_desc),
    global: $g_any,
    kind: 'ref_test',
    valid: ['struct_desc'],
  },
  {
    name: 'desc',
    heap_type: $desc,
    type_code: wasmSignedLeb($desc),
    global: $g_any,
    kind: 'ref_test',
    valid: ['desc'],
  },
  {
    name: 'array',
    heap_type: $array,
    type_code: wasmSignedLeb($array),
    global: $g_any,
    kind: 'ref_test',
    valid: ['array'],
  },
  {
    name: 'func',
    heap_type: $sig,
    type_code: wasmSignedLeb($sig),
    global: $g_func,
    kind: 'ref_test',
    valid: ['func'],
  },
];

for (let cfg of kTypeConfigs) {
  for (let nullable of [true, false]) {
    let fn_name = `check_${cfg.name}_${nullable ? 'null' : 'nonnull'}`;
    let param_type =
        nullable ? wasmRefNullType(cfg.heap_type) : wasmRefType(cfg.heap_type);
    let body;
    if (cfg.kind === 'untestable') {
      if (nullable) {
        body = [kExprI32Const, 1];
      } else {
        body = [kExprLocalGet, 0, kExprRefIsNull, kExprI32Eqz];
      }
    } else if (cfg.kind === 'is_null') {
      body = [kExprLocalGet, 0, kExprRefIsNull];
    } else {
      let test_op = nullable ? kExprRefTestNull : kExprRefTest;
      body = [
        kExprLocalGet,
        0,
        kExprGlobalSet,
        cfg.global.index,
        kExprGlobalGet,
        cfg.global.index,
        kGCPrefix,
        test_op,
        ...cfg.type_code,
      ];
    }
    builder.addFunction(fn_name, makeSig([param_type], [kWasmI32]))
        .addBody(body)
        .exportFunc();
  }
}

let instance = builder.instantiate();
let wasm = instance.exports;

let inputs = {
  null: null,
  js_obj: {},
  smi: 42,
  heapnumber: ((x) => x / 0.5)(21),
  struct: wasm.make_struct(),
  struct_desc: wasm.make_struct_desc(),
  desc: wasm.make_desc(),
  array: wasm.make_array(),
  func: wasm.make_func(),
};

for (let cfg of kTypeConfigs) {
  for (let nullable of [true, false]) {
    let fn_name = `check_${cfg.name}_${nullable ? 'null' : 'nonnull'}`;
    let fn = wasm[fn_name];
    let valid_set = new Set(cfg.valid);
    if (nullable) valid_set.add('null');

    for (let [input_name, input_val] of Object.entries(inputs)) {
      let is_valid = valid_set.has(input_name);
      if (is_valid) {
        assertEquals(1, fn(input_val), `${fn_name}(${input_name})`);
      } else {
        assertThrows(
            () => fn(input_val), TypeError,
            /type incompatibility when transforming from\/to JS/);
      }
    }
  }
}
