// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-gc --wasm-gc-js-interop
// Flags: --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const kIterationsCountForICProgression = 20;

function createArray_i() {
  let builder = new WasmModuleBuilder();

  const type_index = builder.addArray(kWasmI32, true);

  let sig_a_i = makeSig_r_x(kWasmDataRef, kWasmI32);
  let sig_i_ai = makeSig([kWasmDataRef, kWasmI32], [kWasmI32]);
  let sig_v_aii = makeSig([kWasmDataRef, kWasmI32, kWasmI32], []);

  builder.addFunction("new_array", sig_a_i)
    .addBody([
      kExprLocalGet, 0,                             // --
      kExprI32Const, 10,                            // --
      kGCPrefix, kExprRttCanon, type_index,         // --
      kGCPrefix, kExprArrayNewWithRtt, type_index]) // --
    .exportAs("new_array");

  builder.addFunction("array_get", sig_i_ai)
    .addBody([
      kExprLocalGet, 0,                      // --
      kGCPrefix, kExprRttCanon, type_index,  // --
      kGCPrefix, kExprRefCast,               // --
      kExprLocalGet, 1,                      // --
      kGCPrefix, kExprArrayGet, type_index]) // --
    .exportAs("array_get");

  builder.addFunction("array_set", sig_v_aii)
    .addBody([
      kExprLocalGet, 0,                      // --
      kGCPrefix, kExprRttCanon, type_index,  // --
      kGCPrefix, kExprRefCast,               // --
      kExprLocalGet, 1,                      // --
      kExprLocalGet, 2,                      // --
      kGCPrefix, kExprArraySet, type_index]) // --
    .exportAs("array_set");

  let instance = builder.instantiate();
  let new_array = instance.exports.new_array;
  let array_get = instance.exports.array_get;
  let array_set = instance.exports.array_set;

  let value = 42;
  let o = new_array(value);
  %DebugPrint(o);
  assertEquals(10, o.length);
  for (let i = 0; i < o.length; i++) {
    let res;
    res = array_get(o, i);
    assertEquals(value, res);

    array_set(o, i, i);
    res = array_get(o, i);
    assertEquals(i, res);
  }
  return o;
}

(function TestSimpleArrayInterop() {
  function f(o) {
    assertEquals(10, o.length);
    for (let i = 0; i < o.length; i++) {
      let len = o.length;
      assertEquals(10, len);
      let v = o[i];
      assertEquals(i, v);
    }
  }

  let o = createArray_i();
  %DebugPrint(o);

  f(o);
  gc();
})();
