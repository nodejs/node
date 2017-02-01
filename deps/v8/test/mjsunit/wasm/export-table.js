// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function testExportedMain() {
  var kReturnValue = 88;
  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i)
    .addBody([
      kExprI8Const,
      kReturnValue,
      kExprReturn
    ])
    .exportFunc();

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports.main);

  assertEquals(kReturnValue, module.exports.main());
})();

(function testExportedTwice() {
  var kReturnValue = 99;

  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i)
    .addBody([
      kExprI8Const,
      kReturnValue,
      kExprReturn
    ])
    .exportAs("blah")
    .exportAs("foo");

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports.blah);
  assertEquals("function", typeof module.exports.foo);

  assertEquals(kReturnValue, module.exports.foo());
  assertEquals(kReturnValue, module.exports.blah());
})();


(function testNumericName() {
  var kReturnValue = 93;

  var builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i)
    .addBody([
      kExprI8Const,
      kReturnValue,
      kExprReturn
    ])
    .exportAs("0");

  var module = builder.instantiate();

  assertEquals("object", typeof module.exports);
  assertEquals("function", typeof module.exports["0"]);

  assertEquals(kReturnValue, module.exports["0"]());
})();

(function testExportNameClash() {
  var builder = new WasmModuleBuilder();

  builder.addFunction("one",   kSig_v_v).addBody([kExprNop]).exportAs("main");
  builder.addFunction("two",   kSig_v_v).addBody([kExprNop]).exportAs("other");
  builder.addFunction("three", kSig_v_v).addBody([kExprNop]).exportAs("main");

  try {
    builder.instantiate();
    assertUnreachable("should have thrown an exception");
  } catch (e) {
    assertContains("Duplicate export", e.toString());
  }
})();
