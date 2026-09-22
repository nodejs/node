// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors
// (for exact types)

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let instance = (() => {
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let sub_struct = builder.addStruct({
    fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)],
    supertype: struct
  });
  let array = builder.addArray(kWasmF64);
  let sig = builder.addType(makeSig([kWasmI32], [kWasmI32]));

  let func = builder.addFunction('inc', sig)
                 .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
                 .exportAs('inc');

  builder.addFunction('struct_producer', makeSig([], [kWasmStructRef]))
      .addBody([kGCPrefix, kExprStructNewDefault, struct])
      .exportFunc();

  builder.addFunction('sub_struct_producer', makeSig([], [kWasmStructRef]))
      .addBody([kGCPrefix, kExprStructNewDefault, sub_struct])
      .exportFunc();

  builder.addFunction('array_producer', makeSig([], [kWasmArrayRef]))
      .addBody([
        kExprI32Const, 10,
        kGCPrefix, kExprArrayNewDefault, array
      ])
      .exportFunc();

  builder.addFunction('i31_as_eq_producer', makeSig([], [kWasmEqRef]))
      .addBody([kExprI32Const, 5, kGCPrefix, kExprRefI31])
      .exportFunc();

  builder.addFunction('func_producer', makeSig([], [wasmRefType(sig)]))
      .addBody([kExprRefFunc, func.index])
      .exportFunc();

  let test_types = {
    struct: kWasmStructRef,
    array: kWasmArrayRef,
    raw_struct: struct,
    raw_struct_exact: struct,
    raw_sub_struct: sub_struct,
    raw_array: array,
    typed_func: sig,
    i31: kWasmI31Ref,
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
    if (key == 'raw_struct_exact') type = type.exact();
    builder.addFunction(key + '_id', makeSig([type], [type]))
        .addBody([kExprLocalGet, 0])
        .exportFunc();
    let type_encoding = key == 'raw_struct_exact'
        ? [kWasmExact, ...wasmSignedLeb(test_types[key])]
        : wasmSignedLeb(test_types[key]);
    builder.addFunction(key + '_null', makeSig([], [type]))
        .addBody([kExprRefNull, ...type_encoding])
        .exportFunc();
    let non_null_type = wasmRefType(test_types[key]);
    if (key == 'raw_struct_exact') type = type.exact();
    builder.addFunction(key + "_non_null_id",
                        makeSig([non_null_type], [non_null_type]))
        .addBody([kExprLocalGet, 0])
        .exportFunc();
  }

  // Test that the wrappers behave correctly when a type follows an
  // uninhabitable type.
  let uninhabitable = wasmRefType(kWasmNullRef);
  builder.addFunction(
      "multi_value_uninhabitable",
      makeSig([uninhabitable, kWasmI32], [uninhabitable, kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1])
    .exportFunc();

  return builder.instantiate({});
})();

// Wasm-exposed null is the same as JS null.
assertEquals(instance.exports.struct_null(), null);

// We can roundtrip a struct as structref.
instance.exports.struct_id(instance.exports.struct_producer());
// We cannot roundtrip an array as structref.
assertThrows(
    () => instance.exports.struct_id(instance.exports.array_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We can roundtrip null as structref.
instance.exports.struct_id(instance.exports.struct_null());
// We cannot roundtrip an i31 as structref.
assertThrows(
    () => instance.exports.struct_id(instance.exports.i31_as_eq_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip undefined as structref.
assertThrows(
    () => instance.exports.struct_id(undefined),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip a non-smi number as structref.
assertThrows(
    () => instance.exports.struct_id(123.456),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip a JS object as structref.
assertThrows(
    () => instance.exports.struct_id({}),
    TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as non-null structref.
instance.exports.struct_non_null_id(instance.exports.struct_producer());
// We cannot roundtrip an array as non-null structref.
assertThrows(
    () => instance.exports.struct_non_null_id(
        instance.exports.array_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip null as non-null structref.
assertThrows(
    () => instance.exports.struct_non_null_id(instance.exports.struct_null()),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip an i31 as non-null structref.
assertThrows(
    () => instance.exports.struct_non_null_id(
        instance.exports.i31_as_eq_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip undefined as non-null structref.
assertThrows(
    () => instance.exports.struct_non_null_id(undefined),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip a non-smi number as non-null structref.
assertThrows(
    () => instance.exports.struct_non_null_id(123.456),
    TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip a JS object as non-null structref.
assertThrows(
    () => instance.exports.struct_non_null_id({}),
    TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip an array as arrayref.
instance.exports.array_id(instance.exports.array_producer());
// We cannot roundtrip a struct as arrayref.
assertThrows(
    () => instance.exports.array_id(instance.exports.struct_producer()),
    TypeError,
    'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as eqref.
instance.exports.eq_id(instance.exports.struct_producer());
// We can roundtrip an array as eqref.
instance.exports.eq_id(instance.exports.array_producer());
// We can roundtrip an i31 as eqref/i31ref.
instance.exports.eq_id(instance.exports.i31_as_eq_producer());
instance.exports.i31_id(instance.exports.i31_as_eq_producer());
// We can roundtrip a number in range as i31.
instance.exports.i31_id(42);
instance.exports.i31_id(42.0);
const kInt31MaxValue = 0x3fffffff;
const kInt31MinValue = -kInt31MaxValue - 1;
instance.exports.i31_id(kInt31MaxValue);
instance.exports.i31_id(kInt31MinValue);
// We cannot roundtrip a double or a number outside i31 range as i31.
assertThrows(
    () => instance.exports.i31_id(kInt31MaxValue + 1), TypeError,
    'type incompatibility when transforming from/to JS');
assertThrows(
    () => instance.exports.i31_id(kInt31MinValue - 1), TypeError,
    'type incompatibility when transforming from/to JS');
assertThrows(
    () => instance.exports.i31_id(10.5), TypeError,
    'type incompatibility when transforming from/to JS');
assertThrows(
    () => instance.exports.i31_id(-0.0), TypeError,
    'type incompatibility when transforming from/to JS');
// We cannot roundtrip a JS object as i31.
assertThrows(
    () => instance.exports.i31_id({}), TypeError,
    'type incompatibility when transforming from/to JS');
// We can roundtrip any null as any null supertype.
instance.exports.eq_id(instance.exports.struct_null());
instance.exports.eq_id(instance.exports.eq_null());
instance.exports.eq_id(instance.exports.func_null());
instance.exports.eq_id(instance.exports.any_null());
instance.exports.any_id(instance.exports.struct_null());
instance.exports.any_id(instance.exports.eq_null());
instance.exports.any_id(instance.exports.func_null());
instance.exports.any_id(instance.exports.any_null());
instance.exports.i31_id(instance.exports.struct_null());
instance.exports.i31_id(instance.exports.eq_null());
instance.exports.i31_id(instance.exports.func_null());
instance.exports.i31_id(instance.exports.any_null());
instance.exports.struct_id(instance.exports.struct_null());
instance.exports.struct_id(instance.exports.eq_null());
instance.exports.struct_id(instance.exports.func_null());
instance.exports.struct_id(instance.exports.any_null());
// We cannot roundtrip a func as eqref.
assertThrows(
    () => instance.exports.eq_id(instance.exports.func_producer()), TypeError,
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

// We can directly roundtrip structs or arrays.
instance.exports.raw_struct_id(instance.exports.struct_producer());
instance.exports.raw_array_id(instance.exports.array_producer());

// We cannot roundtrip an array as struct and vice versa.
assertThrows(
  () => instance.exports.raw_struct_id(instance.exports.array_producer()),
  TypeError,
  'type incompatibility when transforming from/to JS');
assertThrows(
  () => instance.exports.raw_array_id(instance.exports.struct_producer()),
  TypeError,
  'type incompatibility when transforming from/to JS');

// We can roundtrip a subtype struct as its supertype, but not vice versa.
instance.exports.raw_struct_id(instance.exports.sub_struct_producer());
assertThrows(
  () => instance.exports.raw_sub_struct_id(instance.exports.struct_producer()),
  TypeError,
  'type incompatibility when transforming from/to JS');

// We can roundtrip a struct as an exact type, but not its subtype.
instance.exports.raw_struct_exact_id(instance.exports.struct_producer());
assertThrows(
  () => instance.exports.raw_struct_exact_id(
    instance.exports.sub_struct_producer()),
  TypeError,
  'type incompatibility when transforming from/to JS');

// We can roundtrip an extern.
assertEquals(null, instance.exports.extern_id(instance.exports.extern_null()));

// We can roundtrip null typed as one of the three null types though wasm...
for (const nullType of ["none", "nofunc", "noextern"]) {
  instance.exports[`${nullType}_id`](instance.exports[`${nullType}_null`]());
}
// ... but not their non-nullable (uninhabited) versions.
for (const nullType of ["none", "nofunc", "noextern"]) {
  assertThrows(
    () => instance.exports[`${nullType}_non_null_id`](
        instance.exports[`${nullType}_null`]()),
    TypeError,
    'type incompatibility when transforming from/to JS');
}

assertThrows(() => instance.exports.multi_value_uninhabitable(null, 0),
             TypeError, 'type incompatibility when transforming from/to JS');
