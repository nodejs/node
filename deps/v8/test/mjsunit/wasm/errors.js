// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

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
  var instance = undefined;
  try {
    instance = new WebAssembly.Instance(module(bytes), imports);
  } catch(e) {
    // If we fail at startup.
    if (e instanceof WebAssembly.RuntimeError) {
      throw e;
    }
    // Swallow other instantiation errors because we expect instantiation
    // to succeed but runtime to fail.
    return;
  }
  instance.exports.run();
}

function builder() {
  return new WasmModuleBuilder;
}

function assertCompileError(bytes) {
  assertThrows(() => module(bytes), WebAssembly.CompileError);
}

// default imports to {} so we get LinkError by default, thus allowing us to
// distinguish the TypeError we want to catch
function assertTypeError(bytes, imports = {}) {
  assertThrows(() => instance(bytes, imports), TypeError);
}

function assertLinkError(bytes, imports) {
  assertThrows(() => instance(bytes, imports), WebAssembly.LinkError);
}

function assertRuntimeError(bytes, imports) {
  assertThrows(() => instantiateAndFailAtRuntime(bytes, imports),
    WebAssembly.RuntimeError);
}

function assertConversionError(bytes, imports) {
  assertThrows(() => instantiateAndFailAtRuntime(bytes, imports), TypeError);
}

(function TestDecodingError() {
  assertCompileError("");
  assertCompileError("X");
  assertCompileError("\0x00asm");
})();

(function TestValidationError() {
  assertCompileError(builder().addFunction("f", kSig_i_v).end().toBuffer());
  assertCompileError(builder().addFunction("f", kSig_i_v).addBody([
    kExprReturn
  ]).end().toBuffer());
  assertCompileError(builder().addFunction("f", kSig_v_v).addBody([
    kExprGetLocal, 0
  ]).end().toBuffer());
  assertCompileError(builder().addStart(0).toBuffer());
})();

(function TestLinkingError() {
  let b;

  b = builder();
  b.addImport("foo", "bar", kSig_v_v);
  assertLinkError(b.toBuffer(), {});
  b = builder();
  b.addImport("foo", "bar", kSig_v_v);
  assertLinkError(b.toBuffer(), {foo: {}});
  b = builder();
  b.addImport("foo", "bar", kSig_v_v);
  assertLinkError(b.toBuffer(), {foo: {bar: 9}});

  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(b.toBuffer(), {});
  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(b.toBuffer(), {foo: {}});
  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(b.toBuffer(), {foo: {bar: ""}});
  b = builder();
  b.addImportedGlobal("foo", "bar", kWasmI32);
  assertLinkError(b.toBuffer(), {foo: {bar: () => 9}});

  b = builder();
  b.addImportedMemory("foo", "bar");
  assertLinkError(b.toBuffer(), {});
  b = builder();
  b.addImportedMemory("foo", "bar");
  assertLinkError(b.toBuffer(), {foo: {}});
  b = builder();
  b.addImportedMemory("foo", "bar", 1);
  assertLinkError(b.toBuffer(),
      {foo: {bar: () => new WebAssembly.Memory({initial: 0})}});

  b = builder();
  b.addFunction("startup", kSig_v_v).addBody([
    kExprUnreachable,
  ]).end().addStart(0);
  assertRuntimeError(b.toBuffer());
})();

(function TestTrapError() {
  assertRuntimeError(builder().addFunction("run", kSig_v_v).addBody([
    kExprUnreachable
  ]).exportFunc().end().toBuffer());

  assertRuntimeError(builder().addFunction("run", kSig_v_v).addBody([
    kExprI32Const, 1,
    kExprI32Const, 0,
    kExprI32DivS,
    kExprDrop
  ]).exportFunc().end().toBuffer());

  assertRuntimeError(builder().addFunction("run", kSig_v_v).addBody([
  ]).exportFunc().end().
  addFunction("start", kSig_v_v).addBody([
    kExprUnreachable
  ]).end().addStart(1).toBuffer());
})();

(function TestConversionError() {
  let b = builder();
  b.addImport("foo", "bar", kSig_v_l);
  assertConversionError(b.addFunction("run", kSig_v_v).addBody([
    kExprI64Const, 0, kExprCallFunction, 0
  ]).exportFunc().end().toBuffer(), {foo:{bar: (l)=>{}}});

  b = builder()
  assertConversionError(builder().addFunction("run", kSig_l_v).addBody([
    kExprI64Const, 0
  ]).exportFunc().end().toBuffer());
})();
