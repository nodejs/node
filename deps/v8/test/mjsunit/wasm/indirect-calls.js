// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var module = (function () {
  var builder = new WasmModuleBuilder();

  var sig_index = builder.addSignature([kAstI32, kAstI32, kAstI32]);
  builder.addImport("add", sig_index);
  builder.addFunction("add", sig_index)
    .addBody([
      kExprCallImport, 0, kExprGetLocal, 0, kExprGetLocal, 1
    ]);
  builder.addFunction("sub", sig_index)
    .addBody([
      kExprI32Sub, kExprGetLocal, 0, kExprGetLocal, 1
    ]);
  builder.addFunction("main", [kAstI32, kAstI32, kAstI32, kAstI32])
    .addBody([
      kExprCallIndirect, sig_index,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprGetLocal, 2])
    .exportFunc()
  builder.appendToFunctionTable([0, 1, 2]);

  return builder.instantiate({add: function(a, b) { return a + b | 0; }});
})();

// Check the module exists.
assertFalse(module === undefined);
assertFalse(module === null);
assertFalse(module === 0);
assertEquals("object", typeof module.exports);
assertEquals("function", typeof module.exports.main);

assertEquals(5, module.exports.main(1, 12, 7));
assertEquals(19, module.exports.main(0, 12, 7));

assertTraps(kTrapFuncSigMismatch, "module.exports.main(2, 12, 33)");
assertTraps(kTrapFuncInvalid, "module.exports.main(3, 12, 33)");
