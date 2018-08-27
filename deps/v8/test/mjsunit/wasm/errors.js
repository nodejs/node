// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax

'use strict';

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function module(bytes) {
  let buffer = bytes;
  if (typeof buffer === 'string') {
    buffer = new ArrayBuffer(bytes.length);
    let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; ++i) {
      view[i] = bytes.charCodeAt(i);
    }
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = {}) {
  return new WebAssembly.Instance(module(bytes), imports);
}

// instantiate should succeed but run should fail.
function instantiateAndFailAtRuntime(bytes, imports = {}) {
  var instance =
      assertDoesNotThrow(new WebAssembly.Instance(module(bytes), imports));
  instance.exports.run();
}

function builder() {
  return new WasmModuleBuilder;
}

function assertCompileError(bytes, msg) {
  assertThrows(() => module(bytes), WebAssembly.CompileError, msg);
}

// default imports to {} so we get LinkError by default, thus allowing us to
// distinguish the TypeError we want to catch
function assertTypeError(bytes, imports = {}, msg) {
  assertThrows(() => instance(bytes, imports), TypeError, msg);
}

function assertLinkError(bytes, imports, msg) {
  assertThrows(() => instance(bytes, imports), WebAssembly.LinkError, msg);
}

function assertRuntimeError(bytes, imports, msg) {
  assertThrows(
      () => instantiateAndFailAtRuntime(bytes, imports),
      WebAssembly.RuntimeError, msg);
}

function assertConversionError(bytes, imports, msg) {
  assertThrows(
      () => instantiateAndFailAtRuntime(bytes, imports), TypeError, msg);
}

(function TestDecodingError() {
  assertCompileError("", /is empty/);
  assertCompileError("X", /expected 4 bytes, fell off end @\+0/);
  assertCompileError(
    "\0x00asm", /expected magic word 00 61 73 6d, found 00 78 30 30 @\+0/);
})();

(function TestValidationError() {
  assertCompileError(
      builder().addFunction("f", kSig_i_v).end().toBuffer(),
      /function body must end with "end" opcode @/);
  assertCompileError(builder().addFunction("f", kSig_i_v).addBody([
    kExprReturn
  ]).end().toBuffer(), /return found empty stack @/);
  assertCompileError(builder().addFunction("f", kSig_v_v).addBody([
    kExprGetLocal, 0
  ]).end().toBuffer(), /invalid local index: 0 @/);
  assertCompileError(
      builder().addStart(0).toBuffer(), /function index 0 out of bounds/);
})();

(function TestTypeError() {
  let b;
  b = builder();
  b.addImport("foo", "bar", kSig_v_v);
  assertTypeError(b.toBuffer(), {}, /module is not an object or function/);

  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertTypeError(b.toBuffer(), {}, /module is not an object or function/);

  b = builder();
  b.addImportedMemory("foo", "bar");
  assertTypeError(b.toBuffer(), {}, /module is not an object or function/);
})();

(function TestLinkingError() {
  let b;

  b = builder();
  b.addImport("foo", "bar", kSig_v_v);
  assertLinkError(
      b.toBuffer(), {foo: {}}, /function import requires a callable/);
  b = builder();
  b.addImport("foo", "bar", kSig_v_v);
  assertLinkError(
      b.toBuffer(), {foo: {bar: 9}}, /function import requires a callable/);

  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(b.toBuffer(), {foo: {}}, /global import must be a number/);
  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(
      b.toBuffer(), {foo: {bar: ""}}, /global import must be a number/);
  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(
      b.toBuffer(), {foo: {bar: () => 9}}, /global import must be a number/);

  b = builder();
  b.addImportedMemory("foo", "bar");
  assertLinkError(
      b.toBuffer(), {foo: {}},
      /memory import must be a WebAssembly\.Memory object/);
  b = builder();
  b.addImportedMemory("foo", "bar", 1);
  assertLinkError(
      b.toBuffer(), {foo: {bar: () => new WebAssembly.Memory({initial: 0})}},
      /memory import must be a WebAssembly\.Memory object/);

  b = builder();
  b.addFunction("startup", kSig_v_v).addBody([
    kExprUnreachable,
  ]).end().addStart(0);
  assertRuntimeError(b.toBuffer(), {}, "unreachable");
})();

(function TestTrapError() {
  assertRuntimeError(builder().addFunction("run", kSig_v_v).addBody([
    kExprUnreachable
  ]).exportFunc().end().toBuffer(), {}, "unreachable");

  assertRuntimeError(builder().addFunction("run", kSig_v_v).addBody([
    kExprI32Const, 1,
    kExprI32Const, 0,
    kExprI32DivS,
    kExprDrop
  ]).exportFunc().end().toBuffer(), {}, "divide by zero");

  assertRuntimeError(builder().
      addFunction("run", kSig_v_v).addBody([]).exportFunc().end().
      addFunction("start", kSig_v_v).addBody([kExprUnreachable]).end().
      addStart(1).toBuffer(),
    {}, "unreachable");
})();

(function TestConversionError() {
  let b = builder();
  b.addImport('foo', 'bar', kSig_v_l);
  let buffer = b.addFunction('run', kSig_v_v)
                   .addBody([kExprI64Const, 0, kExprCallFunction, 0])
                   .exportFunc()
                   .end()
                   .toBuffer();
  assertConversionError(
      buffer, {foo: {bar: (l) => {}}}, kTrapMsgs[kTrapTypeError]);

  buffer = builder()
               .addFunction('run', kSig_l_v)
               .addBody([kExprI64Const, 0])
               .exportFunc()
               .end()
               .toBuffer();
  assertConversionError(buffer, {}, kTrapMsgs[kTrapTypeError]);
})();


(function InternalDebugTrace() {
  var builder = new WasmModuleBuilder();
  var sig = builder.addType(kSig_i_dd);
  builder.addImport("mod", "func", sig);
  builder.addFunction("main", sig)
    .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprCallFunction, 0])
    .exportAs("main");
  var main = builder.instantiate({
    mod: {
      func: ()=>{%DebugTrace();}
    }
  }).exports.main;
  main();
})();
