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
  var g = builder.addImportedGlobal("foo", undefined, type);
  builder.addFunction("main", sig)
    .addBody([kExprGetGlobal, g.index])
    .exportAs("main");
  builder.addGlobal(kAstI32);  // pad

  var instance = builder.instantiate({foo: val});
  assertEquals(expected, instance.exports.main());
}

TestImported(kAstI32, 300.1, 300);
TestImported(kAstF32, 87234.87238, Math.fround(87234.87238));
TestImported(kAstF64, 77777.88888, 77777.88888);
TestImported(kAstF64, "89", 89);


function TestExported(type, val, expected) {
  print("TestExported " + type + "(" + val +")" + " = " + expected);
  var builder = new WasmModuleBuilder();
  var sig = makeSig([type], []);
  builder.addGlobal(kAstI32);  // pad
  var g = builder.addGlobal(type, false)
      .exportAs("foo");
  g.init = val;
  builder.addGlobal(kAstI32);  // pad

  var instance = builder.instantiate();
  assertEquals(expected, instance.exports.foo);
}

TestExported(kAstI32, 455.5, 455);
TestExported(kAstF32, -999.34343, Math.fround(-999.34343));
TestExported(kAstF64, 87347.66666, 87347.66666);


function TestImportedExported(type, val, expected) {
  print("TestImportedExported " + type + "(" + val +")" + " = " + expected);
  var builder = new WasmModuleBuilder();
  var sig = makeSig([type], []);
  var i = builder.addImportedGlobal("foo", undefined, type);
  builder.addGlobal(kAstI32);  // pad
  var o = builder.addGlobal(type, false)
      .exportAs("bar");
  o.init_index = i;
  builder.addGlobal(kAstI32);  // pad

  var instance = builder.instantiate({foo: val});
  assertEquals(expected, instance.exports.bar);
}

TestImportedExported(kAstI32, 415.5, 415);
TestImportedExported(kAstF32, -979.34343, Math.fround(-979.34343));
TestImportedExported(kAstF64, 81347.66666, 81347.66666);

function TestGlobalIndexSpace(type, val) {
  print("TestGlobalIndexSpace(" + val + ") = " + val);
  var builder = new WasmModuleBuilder();
  var im = builder.addImportedGlobal("foo", undefined, type);
  assertEquals(0, im);
  var def = builder.addGlobal(type, false);
  assertEquals(1, def.index);
  def.init_index = im;

  var sig = makeSig([], [type]);
  builder.addFunction("main", sig)
    .addBody([kExprGetGlobal, def.index])
    .exportAs("main");

  var instance = builder.instantiate({foo: val});
  assertEquals(val, instance.exports.main());
}

TestGlobalIndexSpace(kAstI32, 123);
TestGlobalIndexSpace(kAstF32, 54321.125);
TestGlobalIndexSpace(kAstF64, 12345.678);
