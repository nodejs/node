// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load('test/mjsunit/wasm/wasm-module-builder.js');

var debug = true;

function instantiate(buffer, ffi) {
  return new WebAssembly.Instance(new WebAssembly.Module(buffer), ffi);
}

(function BasicTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 2, false);
  builder.addFunction('foo', kSig_i_v)
      .addBody([kExprI32Const, 11])
      .exportAs('blarg');

  var buffer = builder.toBuffer(debug);
  var instance = instantiate(buffer);
  assertEquals(11, instance.exports.blarg());
})();

(function ImportTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  var index = builder.addImport('', 'print', makeSig_v_x(kWasmI32));
  builder.addFunction('foo', kSig_v_v)
      .addBody([kExprI32Const, 13, kExprCallFunction, index])
      .exportAs('main');

  var buffer = builder.toBuffer(debug);
  var instance = instantiate(buffer, {'': {print: print}});
  print('should print 13! ');
  instance.exports.main();
})();

(function LocalsTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction(undefined, kSig_i_i)
      .addLocals({i32_count: 1})
      .addBody([kExprGetLocal, 0, kExprSetLocal, 1, kExprGetLocal, 1])
      .exportAs('main');

  var buffer = builder.toBuffer(debug);
  var instance = instantiate(buffer);
  assertEquals(19, instance.exports.main(19));
  assertEquals(27777, instance.exports.main(27777));
})();

(function LocalsTest2() {
  print(arguments.callee.name);
  // TODO(titzer): i64 only works on 64-bit platforms.
  var types = [
    {locals: {i32_count: 1}, type: kWasmI32},
    // {locals: {i64_count: 1}, type: kWasmI64},
    {locals: {f32_count: 1}, type: kWasmF32},
    {locals: {f64_count: 1}, type: kWasmF64},
  ];

  for (p of types) {
    let builder = new WasmModuleBuilder();
    builder.addFunction(undefined, makeSig_r_x(p.type, p.type))
        .addLocals(p.locals)
        .addBody([kExprGetLocal, 0, kExprSetLocal, 1, kExprGetLocal, 1])
        .exportAs('main');

    var buffer = builder.toBuffer(debug);
    var instance = instantiate(buffer);
    assertEquals(19, instance.exports.main(19));
    assertEquals(27777, instance.exports.main(27777));
  }
})();

(function CallTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('add', kSig_i_ii).addBody([
    kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add
  ]);
  builder.addFunction('main', kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprCallFunction, 0])
      .exportAs('main');

  var instance = builder.instantiate();
  assertEquals(44, instance.exports.main(11, 33));
  assertEquals(7777, instance.exports.main(2222, 5555));
})();

(function IndirectCallTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('add', kSig_i_ii).addBody([
    kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add
  ]);
  builder.addFunction('main', kSig_i_iii)
      .addBody([
        kExprGetLocal, 1, kExprGetLocal, 2, kExprGetLocal, 0, kExprCallIndirect,
        0, kTableZero
      ])
      .exportAs('main');
  builder.appendToTable([0]);

  var instance = builder.instantiate();
  assertEquals(44, instance.exports.main(0, 11, 33));
  assertEquals(7777, instance.exports.main(0, 2222, 5555));
  assertThrows(() => instance.exports.main(1, 1, 1));
})();

(function DataSegmentTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.addFunction('load', kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
      .exportAs('load');
  builder.addDataSegment(0, [9, 9, 9, 9]);

  var buffer = builder.toBuffer(debug);
  var instance = instantiate(buffer);
  assertEquals(151587081, instance.exports.load(0));
})();

(function BasicTestWithUint8Array() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 2, false);
  builder.addFunction('foo', kSig_i_v)
      .addBody([kExprI32Const, 17])
      .exportAs('blarg');

  var buffer = builder.toBuffer(debug);
  var array = new Uint8Array(buffer);
  var instance = instantiate(array);
  assertEquals(17, instance.exports.blarg());

  var kPad = 5;
  var buffer2 = new ArrayBuffer(kPad + buffer.byteLength + kPad);
  var whole = new Uint8Array(buffer2);
  for (var i = 0; i < whole.byteLength; i++) {
    whole[i] = 0xff;
  }
  var array2 = new Uint8Array(buffer2, kPad, buffer.byteLength);
  for (var i = 0; i < array2.byteLength; i++) {
    array2[i] = array[i];
  }
  var instance = instantiate(array2);
  assertEquals(17, instance.exports.blarg());
})();

(function ImportTestTwoLevel() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  var index = builder.addImport('mod', 'print', makeSig_v_x(kWasmI32));
  builder.addFunction('foo', kSig_v_v)
      .addBody([kExprI32Const, 19, kExprCallFunction, index])
      .exportAs('main');

  var buffer = builder.toBuffer(debug);
  var instance = instantiate(buffer, {mod: {print: print}});
  print('should print 19! ');
  instance.exports.main();
})();

(function TestI32Const() {
  print(arguments.callee.name);
  let ints = [
    // A few negative number of different length.
    -3 << 28, -20000, -400, -200, -100, -50, -10, -1,
    // And a few positive number of different length.
    0, 1, 2, 20, 120, 130, 260, 500, 5000000, 3 << 28
  ];
  for (let i of ints) {
    let builder = new WasmModuleBuilder();
    builder.addFunction('main', kSig_i_v)
        .addBody([...wasmI32Const(i)])
        .exportAs('main');
    let instance = builder.instantiate();
    assertEquals(i, instance.exports.main());
  }
})();
