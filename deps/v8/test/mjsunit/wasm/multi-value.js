// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function MultiBlockResultTest() {
  print("MultiBlockResultTest");
  let builder = new WasmModuleBuilder();
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprBlock, sig_ii_v,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
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
      kExprLocalGet, 0,
      kExprLocalGet, 1,
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
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprBr, 0,
      kExprEnd,
      kExprI32Add])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 4), 5);
})();

(function MultiBlockUnreachableTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_il_v = builder.addType(makeSig([], [kWasmI32, kWasmI64]));

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprBlock, sig_il_v,
      kExprI32Const, 1,
      kExprI64Const, 1,
      kExprBr, 0,
      kExprI32Const, 1,
      kExprI64Const, 1,
      kExprEnd,
      kExprDrop])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1, 2), 1);
})();

(function MultiBlockUnreachableTypeErrorTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_il_v = builder.addType(makeSig([], [kWasmI32, kWasmI64]));

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprBlock, sig_il_v,
      kExprI32Const, 1,
      kExprI64Const, 1,
      kExprBr, 0,
      kExprI64Const, 1,
      kExprI32Const, 1,
      // Wrong order: expect i32, i64.
      kExprEnd,
      kExprDrop])
    .exportAs("main");

  assertThrows(() => new WebAssembly.Module(builder.toBuffer()),
      WebAssembly.CompileError, /expected type i64, found i32.const/);
})();

(function MultiLoopResultTest() {
  print("MultiLoopResultTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_ii_v = builder.addType(kSig_ii_v);

  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprLoop, sig_ii_v,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
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
      kExprLocalGet, 0,
      kExprLocalGet, 1,
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
    .addBody([kExprLocalGet, 0, kExprLocalGet, 0]);
  builder.addFunction("swap", kSig_ii_ii)
    .addBody([kExprLocalGet, 1, kExprLocalGet, 0]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
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
      kExprLocalGet, 0,
      kExprIf, sig_ii_v,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprElse,
      kExprLocalGet, 1,
      kExprLocalGet, 0,
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
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 0,
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
      kExprLocalGet, 0,
      kExprIf, sig_ii_v,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprBr, 0,
      kExprElse,
      kExprLocalGet, 1,
      kExprLocalGet, 0,
      kExprBr, 0,
      kExprEnd,
      kExprI32Sub])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(8, 3), 5);
  assertEquals(instance.exports.main(0, 3), 3);
})();

(function MultiIfParamOneArmedTest() {
  print("MultiIfParamOneArmedTest");
  let builder = new WasmModuleBuilder();
  let sig_i_i = builder.addType(kSig_i_i);

  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprIf, sig_i_i,
      kExprI32Const, 5,
      kExprI32Add,
      kExprEnd])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(0), 0);
  assertEquals(instance.exports.main(1), 6);
})();

(function MultiIfOneArmedNoTypeCheckTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_i_l = builder.addType(kSig_i_l);

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprI64Const, 0,
      kExprI32Const, 0,
      kExprIf, sig_i_l,
      kExprDrop,
      kExprI32Const, 0,
      kExprEnd]);

  assertThrows(() => new WebAssembly.Module(builder.toBuffer()),
      WebAssembly.CompileError, /expected i32, got i64/);
})();

(function MultiResultTest() {
  print("MultiResultTest");
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let sig_iii_ii = builder.addType(kSig_iii_ii);

  builder.addFunction("callee", kSig_iii_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI32Sub]);
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
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
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Add,
      kExprReturn]);
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
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
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Add,
      kExprBr, 0]);
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
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

(function MultiBrTableTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_ii_v)
    .addBody([
      kExprI32Const, 1, kExprI32Const, 2,
      kExprI32Const, 0,
      kExprBrTable, 1, 0, 0,
    ])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(instance.exports.main(), [1, 2]);
})();

(function MultiUnreachablePolymorphicTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_v_i = builder.addType(kSig_v_i);
  let sig_i_i = builder.addType(kSig_i_i);

  builder.addFunction("block", kSig_v_v)
    .addBody([
      kExprReturn,
      kExprBlock, sig_v_i,
      kExprDrop,
      kExprEnd
    ])
    .exportAs("block");
  builder.addFunction("if_else", kSig_v_v)
    .addBody([
      kExprReturn,
      kExprIf, sig_v_i,
      kExprDrop,
      kExprElse,
      kExprDrop,
      kExprEnd
    ])
    .exportAs("if_else");
  builder.addFunction("loop", kSig_v_v)
    .addBody([
      kExprReturn,
      kExprLoop, sig_i_i,
      kExprEnd,
      kExprDrop
    ])
    .exportAs("loop");
  // TODO(thibaudm): Create eh + mv mjsunit test and add try/catch case.
  let instance = builder.instantiate();
  instance.exports.block();
  instance.exports.if_else();
  instance.exports.loop();
})();

(function MultiWasmToJSReturnTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_fi_if = makeSig([kWasmI32, kWasmF32], [kWasmF32, kWasmI32]);

  builder.addFunction("swap", sig_fi_if)
    .addBody([
      kExprLocalGet, 1,
      kExprLocalGet, 0])
    .exportAs("swap");
  builder.addFunction("addsubmul", kSig_iii_i)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprI32Add,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprI32Sub,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprI32Mul])
    .exportAs("addsubmul");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.swap(0, 1.5), [1.5, 0]);
  assertEquals(instance.exports.swap(2, 3.75), [3.75, 2]);
  assertEquals(instance.exports.addsubmul(4), [8, 0, 16]);
  assertEquals(instance.exports.addsubmul(5), [10, 0, 25]);
})();

(function MultiJSToWasmReturnTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  function swap(x, y) { return [y, x]; }
  function swap_proxy(x, y) {
    return new Proxy([y, x], {
      get: function(obj, prop) { return Reflect.get(obj, prop); },
    });
  }
  function proxy_throw(x, y) {
    return new Proxy([y, x], {
      get: function(obj, prop) {
        if (prop == 1) {
          throw new Error("abc");
        }
        return Reflect.get(obj, prop); },
    });
  }
  function drop_first(x, y) {
    return [y];
  }
  function repeat(x, y) {
    return [x, y, x, y];
  }
  function not_receiver(x, y) {
    return 0;
  }
  function not_iterable(x, y) {
    a = [x, y];
    a[Symbol.iterator] = undefined;
    return a;
  }
  function* generator(x, y) {
    yield x;
    yield y;
  }
  function* generator_throw(x, y) {
    yield x;
    throw new Error("def");
  }

  builder.addImport('imports', 'f', kSig_ii_ii);
  builder.addFunction("main", kSig_ii_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, 0])
    .exportAs("main")

  let module = new WebAssembly.Module(builder.toBuffer());

  var instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : swap } });
  assertEquals(instance.exports.main(1, 2), [2, 1]);
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : swap_proxy } });
  assertEquals(instance.exports.main(1, 2), [2, 1]);
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : generator } });
  assertEquals(instance.exports.main(1, 2), [1, 2]);

  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : drop_first } });
  assertThrows(() => instance.exports.main(1, 2), TypeError, "multi-return length mismatch");
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : repeat } });
  assertThrows(() => instance.exports.main(1, 2), TypeError, "multi-return length mismatch");
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : proxy_throw } });
  assertThrows(() => instance.exports.main(1, 2), Error, "abc");
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : not_receiver } });
  assertThrows(() => instance.exports.main(1, 2), TypeError, /not iterable/);
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : not_iterable } });
  assertThrows(() => instance.exports.main(1, 2), TypeError, /not iterable/);
  instance = new WebAssembly.Instance(module, { 'imports' : { 'f' : generator_throw } });
  assertThrows(() => instance.exports.main(1, 2), Error, "def");
})();
