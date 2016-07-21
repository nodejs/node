// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var kReturnValue = 117;

var module = (function Build() {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1, true);
  builder.addFunction("main", kSig_i)
    .addBody([kExprI8Const, kReturnValue])
    .exportFunc();

  return builder.instantiate();
})();

// Check the module exists.
assertFalse(module === undefined);
assertFalse(module === null);
assertFalse(module === 0);
assertEquals("object", typeof module);

// Check the memory is an ArrayBuffer.
var mem = module.exports.memory;
assertFalse(mem === undefined);
assertFalse(mem === null);
assertFalse(mem === 0);
assertEquals("object", typeof mem);
assertTrue(mem instanceof ArrayBuffer);
for (var i = 0; i < 4; i++) {
  module.exports.memory = 0;  // should be ignored
  assertEquals(mem, module.exports.memory);
}

assertEquals(65536, module.exports.memory.byteLength);

// Check the properties of the main function.
var main = module.exports.main;
assertFalse(main === undefined);
assertFalse(main === null);
assertFalse(main === 0);
assertEquals("function", typeof main);

assertEquals(kReturnValue, main());
