// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let instance = (() => {
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let array = builder.addArray(kWasmF64, true);
  let sig = builder.addType(makeSig([kWasmI32], [kWasmI32]));

  let func = builder.addFunction('inc', sig)
                 .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
                 .exportAs('inc');

  builder.addFunction('struct_producer', makeSig([], [kWasmDataRef]))
      .addBody([kGCPrefix, kExprStructNewDefault, struct])
      .exportFunc();

  builder.addFunction('array_producer', makeSig([], [kWasmDataRef]))
      .addBody([
        kExprI32Const, 10,
        kGCPrefix, kExprArrayNewDefault, array
      ])
      .exportFunc();

  builder.addFunction('i31_as_eq_producer', makeSig([], [kWasmEqRef]))
      .addBody([kExprI32Const, 5, kGCPrefix, kExprI31New])
      .exportFunc();

  builder.addFunction('func_producer', makeSig([], [wasmRefType(sig)]))
      .addBody([kExprRefFunc, func.index])
      .exportFunc();

  let test_types = {
    struct: kWasmDataRef,
    array: kWasmDataRef,
    raw_struct: struct,
    raw_array: array,
    typed_func: sig,
    data: kWasmDataRef,
    eq: kWasmEqRef,
    func: kWasmFuncRef,
    any: kWasmAnyRef,
    extern: kWasmExternRef,
    none: kWasmNullRef,
    nofunc: kWasmNullFuncRef,
    noextern: kWasmNullExternRef,
  };

  for (key in test_types) {
    let type = wasmRefNullType(test_types[key]);
    builder.addFunction(key + '_id', makeSig([type], [type]))
        .addBody([kExprLocalGet, 0])
        .exportFunc();
    builder.addFunction(key + '_null', makeSig([], [type]))
        .addBody([kExprRefNull, ...wasmSignedLeb(test_types[key])])
        .exportFunc();
  }

  return builder.instantiate({});
})();

// Wasm-exposed null is the same as JS null.
assertEquals(instance.exports.struct_null(), null);

// We can roundtrip a struct as dataref.
instance.exports.data_id(instance.exports.struct_producer());
// We can roundtrip an array as dataref.
instance.exports.data_id(instance.exports.array_producer());
// We can roundtrip null as dataref.
instance.exports.data_id(instance.exports.data_null());
// We cannot roundtrip an i31 as dataref.
assertThrows(
    () => instance.exports.data_id(instance.exports.i31_as_eq_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as eqref.
instance.exports.eq_id(instance.exports.struct_producer());
// We can roundtrip an array as eqref.
instance.exports.eq_id(instance.exports.array_producer());
// We can roundtrip an i31 as eqref.
instance.exports.eq_id(instance.exports.i31_as_eq_producer());
// We can roundtrip any null as eqref.
instance.exports.eq_id(instance.exports.data_null());
instance.exports.eq_id(instance.exports.eq_null());
instance.exports.eq_id(instance.exports.func_null());
// We cannot roundtrip a func as eqref.
assertThrows(
    () => instance.exports.eq_id(instance.exports.func_producer()), TypeError,
    'type incompatibility when transforming from/to JS');

// Anyref is not allowed at the JS interface.
assertThrows(
    () => instance.exports.any_null(), TypeError,
    'type incompatibility when transforming from/to JS');
assertThrows(
    () => instance.exports.any_id(), TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip a typed function.
instance.exports.typed_func_id(instance.exports.func_producer());
// We can roundtrip any null as typed funcion.
instance.exports.typed_func_id(instance.exports.struct_null());
// We cannot roundtrip a struct as typed funcion.
assertThrows(
    () => instance.exports.typed_func_id(instance.exports.struct_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');

// We can roundtrip a func.
instance.exports.func_id(instance.exports.func_producer());
// We can roundtrip any null as func.
instance.exports.func_id(instance.exports.struct_null());
// We cannot roundtrip an i31 as func.
assertThrows(
    () => instance.exports.func_id(instance.exports.i31_as_eq_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');

// We cannot directly roundtrip structs or arrays.
// TODO(7748): Switch these tests once we can.
assertThrows(
    () => instance.exports.raw_struct_id(instance.exports.struct_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');
assertThrows(
    () => instance.exports.raw_array_id(instance.exports.array_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');

// We can roundtrip an extern.
assertEquals(null, instance.exports.extern_id(instance.exports.extern_null()));

// The special null types are not allowed on the boundary from/to JS.
for (const nullType of ["none", "nofunc", "noextern"]) {
  assertThrows(
    () => instance.exports[`${nullType}_null`](),
    TypeError, 'type incompatibility when transforming from/to JS');
  assertThrows(
    () => instance.exports[`${nullType}_id`](),
    TypeError, 'type incompatibility when transforming from/to JS');
}
