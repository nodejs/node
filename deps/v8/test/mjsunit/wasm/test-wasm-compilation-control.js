// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --validate-asm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

let buffer = (() => {
  var builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_i_v)
    .addBody([kExprI32Const, 42])
    .exportAs("f");
  return builder.toBuffer();
})();

// allow up to the buffer size
%SetWasmCompileControls(buffer.byteLength, true);
%SetWasmInstantiateControls();
var m = new WebAssembly.Module(buffer);
var i = new WebAssembly.Instance(m);
assertEquals(i.exports.f(), 42);

// the buffer can't compile synchronously, but we allow async compile
%SetWasmCompileControls(buffer.byteLength - 1, true);
// test first that we can't sync-instantiate this module anymore
try {
  i = new WebAssembly.Instance(m);
} catch (e) {
  assertTrue(e instanceof RangeError);
}

//...but we can async-instantiate it
WebAssembly.instantiate(m)
  .then(instance => i = instance,
        assertUnreachable);
%RunMicrotasks();
assertTrue(i instanceof WebAssembly.Instance);

try {
  m = new WebAssembly.Module(buffer);
  assertUnreachable();
} catch (e) {
  assertTrue(e instanceof RangeError);
}

WebAssembly.compile(buffer)
  .then(res => m = res,
        m = null);

%RunMicrotasks();
assertTrue(m instanceof WebAssembly.Module)

WebAssembly.instantiate(buffer)
  .then(res => m = res.module,
        m = null);

%RunMicrotasks();
assertTrue(m instanceof WebAssembly.Module);

// Async compile works, but using the sync instantiate doesn't
i = undefined;
m = undefined;
var ex = undefined;
WebAssembly.compile(buffer)
  .then(mod => {
    m = mod;
    try {
      i = new WebAssembly.Instance(m);
    } catch (e) {
      ex = e;
    }
  },
        e => ex = e);
%RunMicrotasks();
assertTrue(ex instanceof RangeError);
assertEquals(i, undefined);
assertTrue(m instanceof WebAssembly.Module);

// Now block async compile works.
%SetWasmCompileControls(buffer.byteLength - 1, false);
WebAssembly.compile(buffer)
  .then(ex = null,
        e => ex = e);

%RunMicrotasks();
assertTrue(ex instanceof RangeError);

WebAssembly.instantiate(buffer)
  .then(ex = null,
        e => ex = e);
%RunMicrotasks();
assertTrue(ex instanceof RangeError);


// Verify that, for asm-wasm, these controls are ignored.
%SetWasmCompileControls(0, false);
function assertValidAsm(func) {
  assertTrue(%IsAsmWasmCode(func));
}

function assertWasm(expected, func) {
  assertEquals(
    expected, func(undefined, undefined, new ArrayBuffer(1024)).caller());
  assertValidAsm(func);
}

function TestAsmWasmIsUnaffected() {
  "use asm";
  function caller() {
    return 43;
  }

  return {caller: caller};
}

assertWasm(43, TestAsmWasmIsUnaffected);
