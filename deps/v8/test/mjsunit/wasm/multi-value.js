// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-mv

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function MultiBlockResultTest() {
  print("MultiBlockResultTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprBlock, sig_ii_v,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprEnd,
      kExprI32Add])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
})();

(function MultiBlockParamTest() {
  print("MultiBlockParamTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprBlock, sig_i_ii,
      kExprI32Add,
      kExprEnd])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
})();

(function MultiBlockBrTest() {
  print("MultiBlockBrTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprBlock, sig_ii_v,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprBr, 0,
      kExprEnd,
      kExprI32Add])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
})();


(function MultiLoopResultTest() {
  print("MultiLoopResultTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprLoop, sig_ii_v,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprEnd,
      kExprI32Add])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
})();

(function MultiLoopParamTest() {
  print("MultiLoopParamTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprLoop, sig_i_ii,
      kExprI32Add,
      kExprEnd])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
})();

(function MultiLoopBrTest() {
  print("MultiLoopBrTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_i = builder.addType(kSig_ii_i);
  let sig_ii_ii = builder.addType(kSig_ii_ii);

  builder.addFunction("dup", kSig_ii_i)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 0]);
  builder.addFunction("swap", kSig_ii_ii)
    .addBody([kExprGetLocal, 1, kExprGetLocal, 0]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprLoop, sig_ii_ii,
      kExprCallFunction, 1,  // swap
      kExprCallFunction, 0,  // dup
      kExprI32Add,
      kExprCallFunction, 0,  // dup
      kExprI32Const, 20,
      kExprI32LeU,
      kExprBrIf, 0,
      kExprEnd,
      kExprDrop])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(0, instance.exports.main(0, 1));
  assertEquals(16, instance.exports.main(1, 1));
  assertEquals(4, instance.exports.main(3, 1));
  assertEquals(4, instance.exports.main(4, 1));
  assertEquals(0, instance.exports.main(0, 2));
  assertEquals(16, instance.exports.main(1, 2));
  assertEquals(8, instance.exports.main(3, 2));
  assertEquals(8, instance.exports.main(4, 2));
  assertEquals(0, instance.exports.main(0, 3));
  assertEquals(8, instance.exports.main(1, 3));
  assertEquals(12, instance.exports.main(3, 3));
  assertEquals(12, instance.exports.main(4, 3));
  assertEquals(0, instance.exports.main(0, 4));
  assertEquals(8, instance.exports.main(1, 4));
  assertEquals(16, instance.exports.main(3, 4));
  assertEquals(16, instance.exports.main(4, 4));
  assertEquals(3, instance.exports.main(100, 3));
  assertEquals(6, instance.exports.main(3, 100));
})();


(function MultiIfResultTest() {
  print("MultiIfResultTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprIf, sig_ii_v,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprElse,
      kExprGetLocal, 1,
      kExprGetLocal, 0,
      kExprEnd,
      kExprI32Sub])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(8, 3), 5);
  assertEquals(instance.exports.main(0, 3), 3);
})();

(function MultiIfParamTest() {
  print("MultiIfParamTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 0,
      kExprIf, sig_i_ii,
      kExprI32Add,
      kExprElse,
      kExprI32Sub,
      kExprEnd])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
  assertEquals(instance.exports.main(0, 4), -4);
})();

(function MultiIfBrTest() {
  print("MultiIfBrTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprIf, sig_ii_v,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprBr, 0,
      kExprElse,
      kExprGetLocal, 1,
      kExprGetLocal, 0,
      kExprBr, 0,
      kExprEnd,
      kExprI32Sub])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(8, 3), 5);
  assertEquals(instance.exports.main(0, 3), 3);
})();

(function MultiResultTest() {
  print("MultiResultTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_iii_ii = builder.addType(kSig_iii_ii);

  builder.addFunction("callee", kSig_iii_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprI32Sub]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprCallFunction, 0,
      kExprI32Mul,
      kExprI32Add])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(0, 0), 0);
  assertEquals(instance.exports.main(1, 0), 1);
  assertEquals(instance.exports.main(2, 0), 2);
  assertEquals(instance.exports.main(0, 1), -1);
  assertEquals(instance.exports.main(0, 2), -4);
  assertEquals(instance.exports.main(3, 4), -1);
  assertEquals(instance.exports.main(4, 3), 7);
})();

(function MultiReturnTest() {
  print("MultiReturnTest");
  let builder = new WasmModuleBuilder();
  let sig_i_i = builder.addType(kSig_i_i);
  let sig_ii_i = builder.addType(kSig_ii_i);

  builder.addFunction("callee", kSig_ii_i)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 0,
      kExprGetLocal, 0,
      kExprI32Add,
      kExprReturn]);
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprCallFunction, 0,
      kExprI32Mul])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(0), 0);
  assertEquals(instance.exports.main(1), 2);
  assertEquals(instance.exports.main(2), 8);
  assertEquals(instance.exports.main(10), 200);
})();

(function MultiBrReturnTest() {
  print("MultiBrReturnTest");
  let builder = new WasmModuleBuilder();
  let sig_i_i = builder.addType(kSig_i_i);
  let sig_ii_i = builder.addType(kSig_ii_i);

  builder.addFunction("callee", kSig_ii_i)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 0,
      kExprGetLocal, 0,
      kExprI32Add,
      kExprBr, 0]);
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprCallFunction, 0,
      kExprI32Mul])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(0), 0);
  assertEquals(instance.exports.main(1), 2);
  assertEquals(instance.exports.main(2), 8);
  assertEquals(instance.exports.main(10), 200);
})();
