// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

(function testIgnoreCompilationHintsSectionUnlessEnabled() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyDefault,
                             kCompilationHintTierInterpreter,
                             kCompilationHintTierInterpreter)
         .exportFunc();
  let instance = builder.instantiate({mod: {pow: Math.pow}});
  assertEquals(27, instance.exports.upow(3))
})();
