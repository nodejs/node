// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function instantiate(sig, body) {
  var builder = new WasmModuleBuilder();

  var func = builder.addFunction("", sig)
    .addBody(body);

  builder.addStart(func.index);

  return builder.instantiate();
}

function assertFails(sig, body) {
  try {
    var module = instantiate(sig, body);
    print("expected failure, but passes");
    assertFalse(true);
  } catch (expected) {
    print("ok: " + expected);
  }
}

function assertVerifies(sig, body) {
  var module = instantiate(sig, body);
  assertFalse(module === undefined);
  assertFalse(module === null);
  assertFalse(module === 0);
  assertEquals("object", typeof module);
  return module;
}

assertVerifies(kSig_v_v, [kExprNop]);
assertVerifies(kSig_i, [kExprI8Const, 0]);

// Arguments aren't allow to start functions.
assertFails(kSig_i_i, [kExprGetLocal, 0]);
assertFails(kSig_i_ii, [kExprGetLocal, 0]);
assertFails(kSig_i_dd, [kExprGetLocal, 0]);

(function testInvalidIndex() {
  print("testInvalidIndex");
  var builder = new WasmModuleBuilder();

  var func = builder.addFunction("", kSig_v_v)
    .addBody([kExprNop]);

  builder.addStart(func.index + 1);

  assertThrows(builder.instantiate);
})();


(function testTwoStartFuncs() {
  print("testTwoStartFuncs");
  var builder = new WasmModuleBuilder();

  var func = builder.addFunction("", kSig_v_v)
    .addBody([kExprNop]);

  builder.addExplicitSection([kStartSectionCode, 0]);
  builder.addExplicitSection([kStartSectionCode, 0]);

  assertThrows(builder.instantiate);
})();


(function testRun() {
  print("testRun");
  var builder = new WasmModuleBuilder();

  builder.addMemory(12, 12, true);

  var func = builder.addFunction("", kSig_v_v)
    .addBody([kExprI8Const, 0, kExprI8Const, 77, kExprI32StoreMem, 0, 0]);

  builder.addStart(func.index);

  var module = builder.instantiate();
  var memory = module.exports.memory.buffer;
  var view = new Int8Array(memory);
  assertEquals(77, view[0]);
})();

(function testStartFFI() {
  print("testStartFFI");
  var ranned = false;
  var ffi = { foo : function() {
    print("we ranned at stert!");
    ranned = true;
  }};

  var builder = new WasmModuleBuilder();
  var sig_index = builder.addType(kSig_v_v);

  builder.addImport("foo", sig_index);
  var func = builder.addFunction("", sig_index)
    .addBody([kExprCallFunction, 0]);

  builder.addStart(func.index);

  var module = builder.instantiate(ffi);
  assertTrue(ranned);
})();
