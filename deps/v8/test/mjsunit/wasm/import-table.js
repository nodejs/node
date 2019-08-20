// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-module-builder.js");

function addConstFunc(builder, val) {
  return builder.addFunction("const" + val, kSig_i_v)
    .addBody(wasmI32Const(val)).index;
}

function addSigs(builder, pad) {
  for (let i = 0; i < pad; i++) {
    let params = [];
    for (let j = 0; j < i; j++) params.push(kWasmF32);
    builder.addType(makeSig(params, []));
  }

  return { i_v: builder.addType(kSig_i_v) };
}

let kTableSize = 50;

(function TestAliasedImportedTable() {
  print(arguments.callee.name);

  {
    let builder = new WasmModuleBuilder();
    let signums = addSigs(builder, 1);

    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    let f15 = addConstFunc(builder, 15);
    let call = builder.addFunction("call", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprCallIndirect, signums.i_v, kTableZero
        ])
        .exportAs("call");
    let f17 = addConstFunc(builder, 17);
    builder.addExport("f15", f15);
    builder.addExport("f17", f17);
    builder.addElementSegment(0, 15, false, [f15]);
    builder.addElementSegment(0, 1, false, [call.index]);

    var mod1 = builder.toModule();
  }

  {
    let builder = new WasmModuleBuilder();
    let signums = addSigs(builder, 5);  // ensure different sigids

    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    let f15 = builder.addImport("m", "f15", kSig_i_v);
    let f17 = builder.addImport("m", "f17", kSig_i_v);
    let f21 = addConstFunc(builder, 21);
    let call = builder.addFunction("call", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprCallIndirect, signums.i_v, kTableZero
        ])
        .exportAs("call");
    let f26 = addConstFunc(builder, 26);
    builder.addElementSegment(0, 17, false, [f17]);
    builder.addElementSegment(0, 21, false, [f21]);
    builder.addElementSegment(0, 26, false, [f26]);
    builder.addElementSegment(0, 5, false, [call.index]);

    var mod2 = builder.toModule();
  }

  var table = new WebAssembly.Table({initial: kTableSize,
                                     maximum: kTableSize, element: "anyfunc"});
  var i1 = new WebAssembly.Instance(mod1, {m: {table: table}});

  var i2 = new WebAssembly.Instance(mod2,
      {m: {table: table, f15: i1.exports.f15, f17: i1.exports.f17}});

  for (i of [15, 17, 21, 26]) {
    print(i);
    assertEquals(i, i1.exports.call(i));
    assertEquals(i, i2.exports.call(i));
  }
  for (i of [0, 1, 5, 16]) {
    assertThrows(() => i1.exports.call(i));
    assertThrows(() => i2.exports.call(i));
  }
})();

function addConstFuncUsingGlobal(builder, val) {
  let g = builder.addGlobal(kWasmI32, false);
  g.init = val;
  return builder.addFunction("global" + val, kSig_i_v)
    .addBody([kExprGetGlobal, g.index]).index;
}

(function TestAliasedImportedTableInstanceGlobals() {
  print(arguments.callee.name);

  {
    let builder = new WasmModuleBuilder();
    let signums = addSigs(builder, 1);

    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    let f14 = addConstFuncUsingGlobal(builder, 14);
    let call = builder.addFunction("call", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprCallIndirect, signums.i_v, kTableZero
        ])
        .exportAs("call");
    let f18 = addConstFuncUsingGlobal(builder, 18);
    builder.addExport("f14", f14);
    builder.addExport("f18", f18);
    builder.addElementSegment(0, 14, false, [f14]);
    builder.addElementSegment(0, 1, false, [call.index]);

    var mod1 = builder.toModule();
  }

  {
    let builder = new WasmModuleBuilder();
    let signums = addSigs(builder, 3);  // ensure different sigids

    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    let f14 = builder.addImport("m", "f14", kSig_i_v);
    let f18 = builder.addImport("m", "f18", kSig_i_v);
    let f22 = addConstFuncUsingGlobal(builder, 22);
    let call = builder.addFunction("call", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprCallIndirect, signums.i_v, kTableZero
        ])
        .exportAs("call");
    let f28 = addConstFuncUsingGlobal(builder, 28);
    builder.addElementSegment(0, 18, false, [f18]);
    builder.addElementSegment(0, 22, false, [f22]);
    builder.addElementSegment(0, 28, false, [f28]);
    builder.addElementSegment(0, 5, false, [call.index]);

    var mod2 = builder.toModule();
  }

  var table = new WebAssembly.Table({initial: kTableSize,
                                     maximum: kTableSize, element: "anyfunc"});
  var i1 = new WebAssembly.Instance(mod1, {m: {table: table}});

  var i2 = new WebAssembly.Instance(mod2,
      {m: {table: table, f14: i1.exports.f14, f18: i1.exports.f18}});

  for (i of [14, 18, 22, 28]) {
    print(i);
    assertEquals(i, i1.exports.call(i));
    assertEquals(i, i2.exports.call(i));
  }
  for (i of [0, 1, 5, 16]) {
    assertThrows(() => i1.exports.call(i));
    assertThrows(() => i2.exports.call(i));
  }
})();


function addConstFuncUsingMemory(builder, val) {
  var addr = builder.address;
  builder.address += 8;
  var bytes = [val & 0xff, (val>>8) & 0xff, (val>>16) & 0xff, (val>>24) & 0xff];
  builder.addDataSegment(addr, bytes);
  return builder.addFunction("mem" + val, kSig_i_v)
    .addBody([
      ...wasmI32Const(addr),
      kExprI32LoadMem, 0, 0
    ]).index;
}

(function TestAliasedImportedTableInstanceMemories() {
  print(arguments.callee.name);

  {
    let builder = new WasmModuleBuilder();
    builder.address = 8;
    let signums = addSigs(builder, 1);

    builder.addMemory(1, 1, false);
    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    let f13 = addConstFuncUsingMemory(builder, 13);
    let call = builder.addFunction("call", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprCallIndirect, signums.i_v, kTableZero
        ])
        .exportAs("call");
    let f19 = addConstFuncUsingMemory(builder, 19);
    builder.addExport("f13", f13);
    builder.addExport("f19", f19);
    builder.addElementSegment(0, 13, false, [f13]);
    builder.addElementSegment(0, 1, false, [call.index]);

    var mod1 = builder.toModule();
  }

  {
    let builder = new WasmModuleBuilder();
    builder.address = 8;
    let signums = addSigs(builder, 4);  // ensure different sigids

    builder.addMemory(1, 1, false);
    builder.addImportedTable("m", "table", kTableSize, kTableSize);
    let f13 = builder.addImport("m", "f13", kSig_i_v);
    let f19 = builder.addImport("m", "f19", kSig_i_v);
    let f23 = addConstFuncUsingMemory(builder, 23);
    let call = builder.addFunction("call", kSig_i_i)
        .addBody([
          kExprGetLocal, 0,
          kExprCallIndirect, signums.i_v, kTableZero
        ])
        .exportAs("call");
    let f29 = addConstFuncUsingMemory(builder, 29);
    builder.addElementSegment(0, 19, false, [f19]);
    builder.addElementSegment(0, 23, false, [f23]);
    builder.addElementSegment(0, 29, false, [f29]);
    builder.addElementSegment(0, 5, false, [call.index]);

    var mod2 = builder.toModule();
  }

  var table = new WebAssembly.Table({initial: kTableSize,
                                     maximum: kTableSize, element: "anyfunc"});
  var i1 = new WebAssembly.Instance(mod1, {m: {table: table}});

  var i2 = new WebAssembly.Instance(mod2,
      {m: {table: table, f13: i1.exports.f13, f19: i1.exports.f19}});

  for (i of [13, 19, 23, 29]) {
    print(i);
    assertEquals(i, i1.exports.call(i));
    assertEquals(i, i2.exports.call(i));
  }
  for (i of [0, 1, 5, 16]) {
    assertThrows(() => i1.exports.call(i));
    assertThrows(() => i2.exports.call(i));
  }
})();
