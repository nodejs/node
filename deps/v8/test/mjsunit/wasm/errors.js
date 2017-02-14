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

function builder() {
  return new WasmModuleBuilder;
}

function assertCompileError(bytes) {
  assertThrows(() => module(bytes), WebAssembly.CompileError);
}

function assertLinkError(bytes, imports = {}) {
  assertThrows(() => instance(bytes, imports), TypeError);
}

function assertRuntimeError(bytes, imports = {}) {
  assertThrows(() => instance(bytes, imports).exports.run(),
    WebAssembly.RuntimeError);
}

function assertConversionError(bytes, imports = {}) {
  assertThrows(() => instance(bytes, imports).exports.run(), TypeError);
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
  b.addImportWithModule("foo", "bar", kSig_v_v);
  assertLinkError(b.toBuffer(), {});
  b = builder();
  b.addImportWithModule("foo", "bar", kSig_v_v);
  assertLinkError(b.toBuffer(), {foo: {}});
  b = builder();
  b.addImportWithModule("foo", "bar", kSig_v_v);
  assertLinkError(b.toBuffer(), {foo: {bar: 9}});

  b = builder();
  b.addImportedGlobal("foo", "bar", kAstI32);
  assertLinkError(b.toBuffer(), {});
  // TODO(titzer): implement stricter import checks for globals.
  // b = builder();
  // b.addImportedGlobal("foo", "bar", kAstI32);
  // assertLinkError(b.toBuffer(), {foo: {}});
  // b = builder();
  // b.addImportedGlobal("foo", "bar", kAstI32);
  // assertLinkError(b.toBuffer(), {foo: {bar: ""}});
  // b = builder();
  // b.addImportedGlobal("foo", "bar", kAstI32);
  // assertLinkError(b.toBuffer(), {foo: {bar: () => 9}});

  b = builder();
  b.addImportedMemory("foo", "bar");
  assertLinkError(b.toBuffer(), {});
  b = builder();
  b.addImportedMemory("foo", "bar");
  assertLinkError(b.toBuffer(), {foo: {}});
  // TODO(titzer): implement stricter import checks for globals.
  // b = builder();
  // b.addImportedMemory("foo", "bar", 1);
  // assertLinkError(b.toBuffer(),
  //     {foo: {bar: new WebAssembly.Memory({initial: 0})}});
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
  b.addImportWithModule("foo", "bar", kSig_v_l);
  assertConversionError(b.addFunction("run", kSig_v_v).addBody([
    kExprI64Const, 0, kExprCallFunction, 0
  ]).exportFunc().end().toBuffer());
  assertConversionError(builder().addFunction("run", kSig_l_v).addBody([
    kExprI64Const, 0
  ]).exportFunc().end().toBuffer());
})();
