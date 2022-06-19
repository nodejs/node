// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-gc --wasm-gc-js-interop
// Flags: --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const kIterationsCountForICProgression = 20;

function createSimpleStruct(field_type, value1, value2) {
  const builder = new WasmModuleBuilder();

  const is_small_int = (field_type == kWasmI8) || (field_type == kWasmI16);
  const parameter_type = is_small_int ? kWasmI32 : field_type;
  const struct_get_opcode = is_small_int ? kExprStructGetS : kExprStructGet;

  const type_index = builder.addStruct([
      {type: field_type, mutability: true},
  ]);

  let sig_a_t = makeSig_r_x(kWasmDataRef, parameter_type);
  let sig_t_a = makeSig_r_x(parameter_type, kWasmDataRef);
  let sig_v_at = makeSig([kWasmDataRef, parameter_type], []);

  builder.addFunction("new_struct", sig_a_t)
    .addBody([
      kExprLocalGet, 0,                              // --
      kGCPrefix, kExprRttCanon, type_index,          // --
      kGCPrefix, kExprStructNewWithRtt, type_index]) // --
    .exportAs("new_struct");

  builder.addFunction("get_field", sig_t_a)
    .addBody([
      kExprLocalGet, 0,                             // --
      kGCPrefix, kExprRttCanon, type_index,         // --
      kGCPrefix, kExprRefCast,                      // --
      kGCPrefix, struct_get_opcode, type_index, 0]) // --
    .exportAs("get_field");

  builder.addFunction("set_field", sig_v_at)
    .addBody([
      kExprLocalGet, 0,                          // --
      kGCPrefix, kExprRttCanon, type_index,      // --
      kGCPrefix, kExprRefCast,                   // --
      kExprLocalGet, 1,                          // --
      kGCPrefix, kExprStructSet, type_index, 0]) // --
    .exportAs("set_field");

  let instance = builder.instantiate();
  let new_struct = instance.exports.new_struct;
  let get_field = instance.exports.get_field;
  let set_field = instance.exports.set_field;

  // Check that ctor, getter and setter work.
  let o = new_struct(value1);

  let res;
  res = get_field(o);
  assertEquals(value1, res);

  set_field(o, value2);
  res = get_field(o);
  assertEquals(value2, res);

  return {new_struct, get_field, set_field};
}

function SimpleStructInterop(field_type, value_generator,
                             value1 = 42, value2 = 111) {
  const { new_struct, get_field, set_field } =
      createSimpleStruct(field_type, value1, value2);

  function f(o, value) {
    for (let i = 0; i < kIterationsCountForICProgression; i++) {
      let store_IC_exception;
      try { o.$field0 = value; } catch (e) { store_IC_exception = e; }

      let set_field_exception;
      try { set_field(o, value); } catch (e) { set_field_exception = e; };

      // set_field() and store IC should throw the same error at the same time.
      assertEquals(set_field_exception, store_IC_exception);
      if (set_field_exception != undefined) continue;

      let expected = get_field(o);

      let v = o.$field0;
      assertEquals(expected, v);

      // "Clear" the field value.
      set_field(o, value1);
      assertEquals(value1, get_field(o));

      o.$field0 = value;
      assertEquals(expected, get_field(o));

      v = o[i];
      assertEquals(undefined, v);
    }
  }
  // Start collecting feedback from scratch.
  %ClearFunctionFeedback(f);

  let o = new_struct(value1);

  for (const value of value_generator()) {
    print("value: " + value);
    f(o, value);
  }
  gc();
}

function MakeValueGenerator(max, ValueConstructor = Number) {
  return function*() {
    const max_safe_integer_float = 2**23 - 1;

    yield -max;
    yield -max - ValueConstructor(1);
    yield max;
    yield 0.0;
    yield -0.0;
    yield 0n;

    for (let i = 0; i < 10; i++) {
      yield ValueConstructor(Math.floor((Math.random() - 0.5) * Number(max)));
    }

    // Try some non-trivial values.
    yield 153;
    yield 77n;
    yield ValueConstructor(Number.MAX_SAFE_INTEGER);
    yield ValueConstructor(Number.MIN_SAFE_INTEGER);
    yield ValueConstructor(max_safe_integer_float);

    yield { foo:17 };
    yield "boom";
    yield { valueOf() { return 42; } };
    yield { valueOf() { return 15142n; } };
    yield { valueOf() { return BigInt(max); } };
    yield { toString() { return "" + max; } };
    yield { toString() { return "" + (-max); } };
    yield { toString() { return "5421351n"; } };
  };
}

(function TestSimpleStructsInteropI8() {
  const max = 0x7f;
  SimpleStructInterop(kWasmI8, MakeValueGenerator(max));
})();

(function TestSimpleStructsInteropI16() {
  const max = 0x7fff;
  SimpleStructInterop(kWasmI16, MakeValueGenerator(max));
})();

(function TestSimpleStructsInteropI32() {
  const max = 0x7fffffff;
  SimpleStructInterop(kWasmI32, MakeValueGenerator(max));
})();

(function TestSimpleStructsInteropI64() {
  const max = 0x7fffffffffffffn;
  SimpleStructInterop(kWasmI64, MakeValueGenerator(max, BigInt), 42n, 153n);
})();

(function TestSimpleStructsInteropF32() {
  const max = 3.402823466e+38;
  SimpleStructInterop(kWasmF32, MakeValueGenerator(max));
})();

(function TestSimpleStructsInteropF64() {
  const max = 1.7976931348623157e+308;
  SimpleStructInterop(kWasmF64, MakeValueGenerator(max));
})();
