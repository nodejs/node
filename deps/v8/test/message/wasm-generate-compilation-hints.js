// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-generate-compilation-hints --liftoff
// Flags: --allow-natives-syntax --wasm-lazy-compilation --wasm-tiering-budget=5

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

(function TestGenerateCompilationPriorities() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction("inc", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
    .exportFunc();

  builder.addFunction("dec", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub])
    .exportFunc();

  builder.addFunction("double", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul])
    .exportFunc();

  let instance = builder.instantiate();

  for (i = 0; i < 10; i++) {
    assertEquals(11, instance.exports.inc(10));
  }
  // We do not tierup under --wasm-generate-compilation-hints.
  assertTrue(%IsLiftoffFunction(instance.exports.inc));

  assertEquals(9, instance.exports.dec(10));
  assertTrue(%IsLiftoffFunction(instance.exports.dec));

  assertTrue(%IsUncompiledWasmFunction(instance.exports.double));

  %GenerateWasmCompilationHints(instance);
})();

(function TestGenerateInstructionFrequenciesAndCallTargets() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let inc = builder.addFunction("inc", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])

  let dec = builder.addFunction("dec", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub])

  builder.addFunction("caller", kSig_i_ii)
    .addLocals(wasmRefNullType(inc.type_index), 1)
    .addBody([
      kExprLocalGet, 0,
      kExprIf, kWasmVoid,
        kExprRefFunc, inc.index, kExprLocalSet, 2,
      kExprElse,
        kExprRefFunc, dec.index, kExprLocalSet, 2,
      kExprEnd,
      kExprLocalGet, 1, kExprLocalGet, 2, kExprCallRef, inc.type_index,
      kExprLocalGet, 0,
      kExprIf, inc.type_index,
        kExprLocalGet, 1, kExprCallFunction, inc.index,
        kExprI32Add,
      kExprElse,
      kExprEnd
    ])
    .exportFunc();

  builder.addDeclarativeElementSegment([inc.index, dec.index]);

  let instance = builder.instantiate();

  assertEquals(9, instance.exports.caller(0, 10));
  assertEquals(9, instance.exports.caller(0, 10));
  assertEquals(9, instance.exports.caller(0, 10));
  assertEquals(9, instance.exports.caller(0, 10));
  assertEquals(22, instance.exports.caller(1, 10));
  assertEquals(22, instance.exports.caller(1, 10));

  %GenerateWasmCompilationHints(instance);
})();
