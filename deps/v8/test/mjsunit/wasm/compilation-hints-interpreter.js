// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --wasm-lazy-validation

load('test/mjsunit/wasm/wasm-module-builder.js');

(function testInterpreterCallsLazyFunctionInOtherInstance() {
  print(arguments.callee.name);
  let builder0 = new WasmModuleBuilder();
  builder0.addFunction("getX", kSig_i_v)
          .addBody([kExprI32Const, 42])
          .setCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierBaseline,
                              kCompilationHintTierBaseline)
          .exportFunc();
  let builder1 = new WasmModuleBuilder();
  builder1.addImport("otherModule", "getX", kSig_i_v);
  builder1.addFunction("plusX", kSig_i_i)
          .addBody([kExprCallFunction, 0,
                    kExprLocalGet, 0,
                    kExprI32Add])
          .setCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierInterpreter,
                              kCompilationHintTierInterpreter)
          .exportFunc();
  let instance0 = builder0.instantiate();
  let instance1 = builder1.instantiate(
      {otherModule: {getX: instance0.exports.getX}});
  assertEquals(46, instance1.exports.plusX(4));
})();

(function testInterpreterCallsLazyBadFunctionInOtherInstance() {
  print(arguments.callee.name);
  let builder0 = new WasmModuleBuilder();
  builder0.addFunction("getX", kSig_i_v)
          .addBody([kExprI64Const, 42])
          .setCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierBaseline,
                              kCompilationHintTierBaseline)
          .exportFunc();
  let builder1 = new WasmModuleBuilder();
  builder1.addImport("otherModule", "getX", kSig_i_v);
  builder1.addFunction("plusX", kSig_i_i)
          .addBody([kExprCallFunction, 0,
                    kExprLocalGet, 0,
                    kExprI32Add])
          .setCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierInterpreter,
                              kCompilationHintTierInterpreter)
          .exportFunc();
  let instance0 = builder0.instantiate();
  let instance1 = builder1.instantiate(
      {otherModule: {getX: instance0.exports.getX}});
  assertThrows(() => instance1.exports.plusX(4),
               WebAssembly.CompileError,
               "Compiling function #0:\"getX\" failed: type error in " +
               "merge[0] (expected i32, got i64) @+57");
})();

(function testInterpreterCallsLazyFunctionThroughIndirection() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let add = builder.addFunction('add', sig_i_ii)
                   .addBody([kExprLocalGet, 0,
                             kExprLocalGet, 1,
                             kExprI32Add])
                   .setCompilationHint(kCompilationHintStrategyLazy,
                                       kCompilationHintTierInterpreter,
                                       kCompilationHintTierInterpreter);
  builder.appendToTable([add.index]);
  builder.addFunction('main', kSig_i_iii)
         .addBody([// Call indirect #0 with args <#1, #2>.
                   kExprLocalGet, 1,
                   kExprLocalGet, 2,
                   kExprLocalGet, 0,
                   kExprCallIndirect, sig_i_ii, kTableZero])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierInterpreter,
                             kCompilationHintTierInterpreter)
         .exportFunc();
  assertEquals(99, builder.instantiate().exports.main(0, 22, 77));
})();

(function testInterpreterCallsLazyBadFunctionThroughIndirection() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_i_ii = builder.addType(kSig_i_ii);
  let add = builder.addFunction('add', sig_i_ii)
                   .addBody([kExprLocalGet, 0,
                             kExprLocalGet, 1,
                             kExprI64Add])
                   .setCompilationHint(kCompilationHintStrategyLazy,
                                       kCompilationHintTierInterpreter,
                                       kCompilationHintTierInterpreter);
  builder.appendToTable([add.index]);
  builder.addFunction('main', kSig_i_iii)
         .addBody([// Call indirect #0 with args <#1, #2>.
                   kExprLocalGet, 1,
                   kExprLocalGet, 2,
                   kExprLocalGet, 0,
                   kExprCallIndirect, sig_i_ii, kTableZero])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierInterpreter,
                             kCompilationHintTierInterpreter)
         .exportFunc();
  assertThrows(() => builder.instantiate().exports.main(0, 22, 77),
               WebAssembly.CompileError,
               "Compiling function #0:\"add\" failed: i64.add[1] expected " +
               "type i64, found local.get of type i32 @+83");
})();
