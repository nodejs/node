// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function testDecodeCompilationHintsSectionNoDowngrade() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierOptimized,
                             kCompilationHintTierBaseline)
         .exportFunc();
  assertThrows(() => builder.instantiate({mod: {pow: Math.pow}}),
               WebAssembly.CompileError,
               "WebAssembly.Module(): Invalid compilation hint 0x19 " +
               "(forbidden downgrade) @+70");
})();

(function testDecodeCompilationHintsSectionNoTiering() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyDefault,
                             kCompilationHintTierBaseline,
                             kCompilationHintTierBaseline)
         .exportFunc();
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow3', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
  let instance = builder.instantiate({mod: {pow: Math.pow}});
  assertEquals(27, instance.exports.upow(3))
})();

(function testDecodeCompilationHintsSectionUpgrade() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow3', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyEager,
                             kCompilationHintTierBaseline,
                             kCompilationHintTierOptimized)
         .exportFunc();
  let instance = builder.instantiate({mod: {pow: Math.pow}});
  assertEquals(27, instance.exports.upow(3))
})();

(function testDecodeCompilationHintsSectionNoImport() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyEager,
                             kCompilationHintTierDefault,
                             kCompilationHintTierOptimized)
         .exportFunc();
  let instance = builder.instantiate();
  assertEquals(9, instance.exports.sq(3))
})();

(function testDecodeCompilationHintsSectionNoExport() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyEager,
                             kCompilationHintTierDefault,
                             kCompilationHintTierOptimized);
  builder.instantiate();
})();

(function testDecodeCompilationHintsSectionTopTierDefault() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyEager,
                             kCompilationHintTierOptimized,
                             kCompilationHintTierDefault)
         .exportFunc();
  let instance = builder.instantiate();
  assertEquals(9, instance.exports.sq(3))
})();

(function testDecodeCompilationHintsLazyBaselineEagerTopTier() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyLazyBaselineEagerTopTier,
                             kCompilationHintTierOptimized,
                             kCompilationHintTierDefault)
         .exportFunc();
  builder.instantiate();
})();

(function testDecodeIllegalCompilationHintBaselineTier() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let kIllegalHintTier = 0x03;
  builder.addFunction('func', kSig_i_i)
      .addBody([kExprUnreachable])
      .setCompilationHint(
          kCompilationHintStrategyDefault, kIllegalHintTier,
          kCompilationHintTierDefault);
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      new RegExp(
          'WebAssembly.Module\\(\\): Invalid compilation hint 0x0c ' +
          '\\(invalid tier 0x03\\)'));
})();

(function testDecodeIllegalCompilationHintTopTier() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let kIllegalHintTier = 0x03;
  builder.addFunction('func', kSig_i_i)
      .addBody([kExprUnreachable])
      .setCompilationHint(
          kCompilationHintStrategyDefault, kCompilationHintTierDefault,
          kIllegalHintTier);
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      new RegExp(
          'WebAssembly.Module\\(\\): Invalid compilation hint 0x30 ' +
          '\\(invalid tier 0x03\\)'));
})();
