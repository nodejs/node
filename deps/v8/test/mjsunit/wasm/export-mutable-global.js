// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

(function exportImmutableGlobal() {
  var builder = new WasmModuleBuilder();
  let globals = [
    [kWasmI32, 'i32_noinit'],
    [kWasmI32, 'i32', 4711],
    [kWasmF32, 'f32_noinit'],
    [kWasmF32, 'f32', Math.fround(3.14)],
    [kWasmF64, 'f64_noinit'],
    [kWasmF64, 'f64', 1 / 7]
  ];
  for (let [type, name, value] of globals) {
    let global_builder = builder.addGlobal(type, false).exportAs(name);
    if (value) global_builder.init = value;
  }
  var instance = builder.instantiate();

  for (let [type, name, value] of globals) {
    let obj = instance.exports[name];
    assertEquals("object", typeof obj, name);
    assertTrue(obj instanceof WebAssembly.Global, name);
    assertEquals(value || 0, obj.value, name);
    assertThrows(() => obj.value = 0);
  }
})();

(function canExportI64Global() {
  var builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI64, false).exportAs('g');
  builder.instantiate();
})();

(function canExportAndImportI64() {
  var builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI64, false).exportAs('g');
  let g = builder.instantiate().exports.g;

  builder = new WasmModuleBuilder();
  builder.addImportedGlobal("mod", "g", kWasmI64);
  builder.instantiate({mod: {g: g}});
})();

(function exportMutableGlobal() {
  var builder = new WasmModuleBuilder();
  let globals = [
    [kWasmI32, 'i32_noinit'],              // -
    [kWasmI32, 'i32', 4711],               // -
    [kWasmF32, 'f32_noinit'],              // -
    [kWasmF32, 'f32', Math.fround(3.14)],  // -
    [kWasmF64, 'f64_noinit'],              // -
    [kWasmF64, 'f64', 1 / 7]               // -
  ];
  for (let [index, [type, name, value]] of globals.entries()) {
    let global_builder = builder.addGlobal(type, true).exportAs(name);
    if (value) global_builder.init = value;
    builder.addFunction("get " + name, makeSig([], [type]))
      .addBody([kExprGetGlobal, index])
      .exportFunc();
    builder.addFunction("set " + name, makeSig([type], []))
      .addBody([kExprGetLocal, 0, kExprSetGlobal, index])
      .exportFunc();
  }
  var instance = builder.instantiate();

  for (let [type, name, value] of globals) {
    let obj = instance.exports[name];

    assertEquals(value || 0, obj.value, name);

    // Changing the exported global should change the instance's global.
    obj.value = 1001;
    assertEquals(1001, instance.exports['get ' + name](), name);

    // Changing the instance's global should change the exported global.
    instance.exports['set ' + name](112358);
    assertEquals(112358, obj.value, name);
  }
})();

(function exportImportedMutableGlobal() {
  let builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true).exportAs('g1');
  let g1 = builder.instantiate().exports.g1;

  builder = new WasmModuleBuilder();
  builder.addImportedGlobal("mod", "g1", kWasmI32, true);
  builder.addExportOfKind('g2', kExternalGlobal, 0);
  let g2 = builder.instantiate({mod: {g1: g1}}).exports.g2;

  g1.value = 123;

  assertEquals(g1.value, g2.value);
})();
