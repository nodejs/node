// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function testExportedMain() {
  var kReturnValue = 88;
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", [kAstI32])
    .addBody([
      kExprReturn,
      kExprI8Const,
      kReturnValue])
    .exportFunc();

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports.main);

  assertEquals(kReturnValue, module.exports.main());
})();

(function testExportedTwice() {
  var kReturnValue = 99;

  var builder = new WasmModuleBuilder();

  builder.addFunction("main", [kAstI32])
    .addBody([
      kExprReturn,
      kExprI8Const,
      kReturnValue])
    .exportAs("blah")
    .exportAs("foo");

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports.blah);
  assertEquals("function", typeof module.exports.foo);

  assertEquals(kReturnValue, module.exports.foo());
  assertEquals(kReturnValue, module.exports.blah());
})();
