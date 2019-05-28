// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

load('test/mjsunit/wasm/wasm-module-builder.js');

(function testDecodeCompilationHintsSectionNoDowngrade() {
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .giveCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierOptimized,
                              kCompilationHintTierBaseline)
         .exportFunc();
  assertThrows(() => builder.instantiate({mod: {pow: Math.pow}}),
               WebAssembly.CompileError,
               "WebAssembly.Module(): Invalid compilation hint 0x2d " +
               "(forbidden downgrade) @+70");
})();

(function testDecodeCompilationHintsSectionNoTiering() {
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .giveCompilationHint(kCompilationHintStrategyDefault,
                              kCompilationHintTierInterpreter,
                              kCompilationHintTierInterpreter)
         .exportFunc();
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow3', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  let instance = builder.instantiate({mod: {pow: Math.pow}});
  assertEquals(27, instance.exports.upow(3))
})();

(function testDecodeCompilationHintsSectionUpgrade() {
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow3', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .giveCompilationHint(kCompilationHintStrategyEager,
                              kCompilationHintTierBaseline,
                              kCompilationHintTierOptimized)
         .exportFunc();
  let instance = builder.instantiate({mod: {pow: Math.pow}});
  assertEquals(27, instance.exports.upow(3))
})();

(function testDecodeCompilationHintsSectionNoImport() {
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprI32Mul])
         .giveCompilationHint(kCompilationHintStrategyEager,
                              kCompilationHintTierDefault,
                              kCompilationHintTierOptimized)
         .exportFunc();
  let instance = builder.instantiate();
  assertEquals(9, instance.exports.sq(3))
})();

(function testDecodeCompilationHintsSectionNoExport() {
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprI32Mul])
         .giveCompilationHint(kCompilationHintStrategyEager,
                              kCompilationHintTierDefault,
                              kCompilationHintTierOptimized)
  builder.instantiate();
})();

(function testDecodeCompilationHintsSectionTopTierDefault() {
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprI32Mul])
         .giveCompilationHint(kCompilationHintStrategyEager,
                              kCompilationHintTierOptimized,
                              kCompilationHintTierDefault)
         .exportFunc();
  let instance = builder.instantiate();
  assertEquals(9, instance.exports.sq(3))
})();

(function testDecodeCompilationHintsInvalidStrategy() {
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprI32Mul])
         .giveCompilationHint(0x3,
                              kCompilationHintTierOptimized,
                              kCompilationHintTierDefault)
         .exportFunc();
  assertThrows(() => builder.instantiate(),
               WebAssembly.CompileError,
               "WebAssembly.Module(): Invalid compilation hint 0xf " +
               "(unknown strategy) @+49");
})();
