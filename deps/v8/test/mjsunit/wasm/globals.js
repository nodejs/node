// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function TestImported(type, val, expected) {
  print("TestImported " + type + "(" + val +")" + " = " + expected);
  var builder = new WasmModuleBuilder();
  var sig = makeSig([], [type]);
  var g = builder.addImportedGlobal("uuu", "foo", type);
  builder.addFunction("main", sig)
    .addBody([kExprGetGlobal, g])
    .exportAs("main");
  builder.addGlobal(kWasmI32);  // pad

  var instance = builder.instantiate({uuu: {foo: val}});
  assertEquals(expected, instance.exports.main());
}

TestImported(kWasmI32, 300.1, 300);
TestImported(kWasmF32, 87234.87238, Math.fround(87234.87238));
TestImported(kWasmF64, 77777.88888, 77777.88888);


function TestExported(type, val, expected) {
  print("TestExported " + type + "(" + val +")" + " = " + expected);
  var builder = new WasmModuleBuilder();
  var sig = makeSig([type], []);
  builder.addGlobal(kWasmI32);  // pad
  var g = builder.addGlobal(type, false)
      .exportAs("foo");
  g.init = val;
  builder.addGlobal(kWasmI32);  // pad

  var instance = builder.instantiate();
  assertEquals(expected, instance.exports.foo);
}

TestExported(kWasmI32, 455.5, 455);
TestExported(kWasmF32, -999.34343, Math.fround(-999.34343));
TestExported(kWasmF64, 87347.66666, 87347.66666);

(function TestI64Exported() {
  var builder = new WasmModuleBuilder();
  var sig = makeSig([kWasmI64], []);
  builder.addGlobal(kWasmI32);  // pad
  var g = builder.addGlobal(kWasmI64, false)
      .exportAs("foo");
  g.init = 1234;
  builder.addGlobal(kWasmI32);  // pad

  assertThrows(()=> {builder.instantiate()}, WebAssembly.LinkError);
})();

function TestImportedExported(type, val, expected) {
  print("TestImportedExported " + type + "(" + val +")" + " = " + expected);
  var builder = new WasmModuleBuilder();
  var sig = makeSig([type], []);
  var i = builder.addImportedGlobal("ttt", "foo", type);
  builder.addGlobal(kWasmI32);  // pad
  var o = builder.addGlobal(type, false)
      .exportAs("bar");
  o.init_index = i;
  builder.addGlobal(kWasmI32);  // pad

  var instance = builder.instantiate({ttt: {foo: val}});
  assertEquals(expected, instance.exports.bar);
}

TestImportedExported(kWasmI32, 415.5, 415);
TestImportedExported(kWasmF32, -979.34343, Math.fround(-979.34343));
TestImportedExported(kWasmF64, 81347.66666, 81347.66666);

function TestGlobalIndexSpace(type, val) {
  print("TestGlobalIndexSpace(" + val + ") = " + val);
  var builder = new WasmModuleBuilder();
  var im = builder.addImportedGlobal("nnn", "foo", type);
  assertEquals(0, im);
  var def = builder.addGlobal(type, false);
  assertEquals(1, def.index);
  def.init_index = im;

  var sig = makeSig([], [type]);
  builder.addFunction("main", sig)
    .addBody([kExprGetGlobal, def.index])
    .exportAs("main");

  var instance = builder.instantiate({nnn: {foo: val}});
  assertEquals(val, instance.exports.main());
}

TestGlobalIndexSpace(kWasmI32, 123);
TestGlobalIndexSpace(kWasmF32, 54321.125);
TestGlobalIndexSpace(kWasmF64, 12345.678);
