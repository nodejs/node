// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var module = (function () {
  var builder = new WasmModuleBuilder();

  var sig_index = builder.addType(kSig_i_ii);
  builder.addImport("add", sig_index);
  builder.addFunction("add", sig_index)
    .addBody([
      kExprGetLocal, 0, kExprGetLocal, 1, kExprCallFunction, 0
    ]);
  builder.addFunction("sub", sig_index)
    .addBody([
      kExprGetLocal, 0,             // --
      kExprGetLocal, 1,             // --
      kExprI32Sub,                  // --
    ]);
  builder.addFunction("main", kSig_i_iii)
    .addBody([
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kExprGetLocal, 0,
      kExprCallIndirect, sig_index
    ])
    .exportFunc()
  builder.appendToTable([1, 2, 3]);

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


module = (function () {
  var builder = new WasmModuleBuilder();

  var sig_i_ii = builder.addType(kSig_i_ii);
  var sig_i_i = builder.addType(kSig_i_i);
  builder.addImport("mul", sig_i_ii);
  builder.addFunction("add", sig_i_ii)
    .addBody([
      kExprGetLocal, 0,  // --
      kExprGetLocal, 1,  // --
      kExprI32Add        // --
    ]);
  builder.addFunction("popcnt", sig_i_i)
    .addBody([
      kExprGetLocal, 0,  // --
      kExprI32Popcnt     // --
    ]);
  builder.addFunction("main", kSig_i_iii)
    .addBody([
      kExprGetLocal, 1,
      kExprGetLocal, 2,
      kExprGetLocal, 0,
      kExprCallIndirect, sig_i_ii
    ])
    .exportFunc()
  builder.appendToTable([0, 1, 2, 3]);

  return builder.instantiate({mul: function(a, b) { return a * b | 0; }});
})();

assertEquals(-6, module.exports.main(0, -2, 3));
assertEquals(99, module.exports.main(1, 22, 77));
assertTraps(kTrapFuncSigMismatch, "module.exports.main(2, 12, 33)");
assertTraps(kTrapFuncSigMismatch, "module.exports.main(3, 12, 33)");
assertTraps(kTrapFuncInvalid, "module.exports.main(4, 12, 33)");
