// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

load('test/mjsunit/wasm/wasm-module-builder.js');

let instance = (() => {
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let array = builder.addArray(kWasmF64, true);
  let sig = builder.addType(makeSig([kWasmI32], [kWasmI32]));

  let func = builder.addFunction('inc', sig)
                 .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
                 .exportAs('inc');

  builder.addFunction('struct_producer', makeSig([], [kWasmDataRef]))
      .addBody([
        kGCPrefix, kExprRttCanon, struct, kGCPrefix, kExprStructNewDefault,
        struct
      ])
      .exportFunc();

  builder.addFunction('array_producer', makeSig([], [kWasmDataRef]))
      .addBody([
        kExprI32Const, 10, kGCPrefix, kExprRttCanon, array, kGCPrefix,
        kExprArrayNewDefault, array
      ])
      .exportFunc();

  builder.addFunction('i31_producer', makeSig([], [kWasmI31Ref]))
      .addBody([kExprI32Const, 5, kGCPrefix, kExprI31New])
      .exportFunc();

  builder.addFunction('func_producer', makeSig([], [wasmRefType(sig)]))
      .addBody([kExprRefFunc, func.index])
      .exportFunc();

  let test_types = {
    i31: kWasmI31Ref,
    struct: kWasmDataRef,
    array: kWasmDataRef,
    raw_struct: struct,
    raw_array: array,
    typed_func: sig,
    data: kWasmDataRef,
    eq: kWasmEqRef,
    func: kWasmFuncRef,
    any: kWasmAnyRef,
  };

  for (key in test_types) {
    let type = wasmOptRefType(test_types[key]);
    builder.addFunction(key + '_id', makeSig([type], [type]))
        .addBody([kExprLocalGet, 0])
        .exportFunc();
    builder.addFunction(key + '_null', makeSig([], [type]))
        .addBody([kExprRefNull, test_types[key]])
        .exportFunc();
  }

  return builder.instantiate({});
})();

// Wasm-exposed null is the same as JS null.
assertEquals(instance.exports.struct_null(), null);

// We can roundtrip an i31.
instance.exports.i31_id(instance.exports.i31_producer());
// We can roundtrip any null as i31.
instance.exports.i31_id(instance.exports.i31_null());
instance.exports.i31_id(instance.exports.struct_null());
// We cannot roundtrip a struct as i31.
assertThrows(
    () => instance.exports.i31_id(instance.exports.struct_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as dataref.
instance.exports.data_id(instance.exports.struct_producer());
// We can roundtrip an array as dataref.
instance.exports.data_id(instance.exports.array_producer());
// We can roundtrip any null as dataref.
instance.exports.data_id(instance.exports.data_null());
instance.exports.data_id(instance.exports.i31_null());
// We cannot roundtrip an i31 as dataref.
assertThrows(
    () => instance.exports.data_id(instance.exports.i31_producer()), TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as eqref.
instance.exports.eq_id(instance.exports.struct_producer());
// We can roundtrip an array as eqref.
instance.exports.eq_id(instance.exports.array_producer());
// We can roundtrip an i31 as eqref.
instance.exports.eq_id(instance.exports.i31_producer());
// We can roundtrip any null as eqref.
instance.exports.eq_id(instance.exports.data_null());
instance.exports.eq_id(instance.exports.i31_null());
instance.exports.eq_id(instance.exports.func_null());
// We cannot roundtrip a func as eqref.
assertThrows(
    () => instance.exports.eq_id(instance.exports.func_producer()), TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as anyref.
instance.exports.any_id(instance.exports.struct_producer());
// We can roundtrip an array as anyref.
instance.exports.any_id(instance.exports.array_producer());
// We can roundtrip an i31 as anyref.
instance.exports.any_id(instance.exports.i31_producer());
// We can roundtrip a func as anyref.
instance.exports.any_id(instance.exports.func_producer());
// We can roundtrip any null as anyref.
instance.exports.any_id(instance.exports.data_null());
instance.exports.any_id(instance.exports.i31_null());
instance.exports.any_id(instance.exports.func_null());
// We can roundtrip a JS object as anyref.
instance.exports.any_id(instance);

// We can roundtrip a typed function.
instance.exports.typed_func_id(instance.exports.func_producer());
// We can roundtrip any null as typed funcion.
instance.exports.typed_func_id(instance.exports.i31_null());
instance.exports.typed_func_id(instance.exports.struct_null());
// We cannot roundtrip a struct as typed funcion.
assertThrows(
    () => instance.exports.typed_func_id(instance.exports.struct_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');

// We can roundtrip a func.
instance.exports.func_id(instance.exports.func_producer());
// We can roundtrip any null as func.
instance.exports.func_id(instance.exports.i31_null());
instance.exports.func_id(instance.exports.struct_null());
// We cannot roundtrip an i31 as func.
assertThrows(
    () => instance.exports.func_id(instance.exports.i31_producer()), TypeError,
    'type incompatibility when transforming from/to JS');

// We cannot directly roundtrip structs or arrays.
// TODO(7748): Switch these tests once we can.
assertThrows(
    () => instance.exports.raw_struct_id(instance.exports.struct_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');
assertThrows(
    () => instance.exports.raw_array_id(instance.exports.array_producer()),
    TypeError, 'type incompatibility when transforming from/to JS');
