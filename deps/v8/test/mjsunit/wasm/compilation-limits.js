// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

%SetWasmCompileControls(100000, true);
%SetWasmCompileControls(100000, false);

let buffer = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_i_v)
    .addBody([kExprI32Const, 42])
    .exportAs("f");
  return builder.toBuffer();
})();

let ok_module = new WebAssembly.Module(buffer);
assertTrue(ok_module instanceof WebAssembly.Module);
assertEquals(42, new WebAssembly.Instance(ok_module).exports.f());

failWithMessage = msg => %AbortJS(msg);

async function SuccessfulTest() {
  print("SuccessfulTest...");
  %SetWasmCompileControls(buffer.byteLength, true);
  %SetWasmInstantiateControls();
  let m = new WebAssembly.Module(buffer);
  let i = new WebAssembly.Instance(m);
  assertEquals(i.exports.f(), 42);
}

async function FailSyncCompile() {
  print("FailSyncCompile...");
  %SetWasmCompileControls(buffer.byteLength - 1, true);
  assertThrows(() => new WebAssembly.Module(buffer), RangeError);

  print("  wait");
  try {
    let m = await WebAssembly.compile(buffer);
    print("  cont");
    assertTrue(m instanceof WebAssembly.Module);
  } catch (e) {
    print("  catch");
    assertUnreachable();
  }
}

async function FailSyncInstantiate() {
  print("FailSyncInstantiate...");
  %SetWasmCompileControls(buffer.byteLength - 1, true);
  assertThrows(() => new WebAssembly.Instance(ok_module), RangeError);

  print("  wait");
  try {
    let i = await WebAssembly.instantiate(ok_module);
    print("  cont");
    assertTrue(i instanceof WebAssembly.Instance);
  } catch (e) {
    print("  catch: " + e);
    assertUnreachable();
  }
}

async function TestAll() {
  await SuccessfulTest();
  await FailSyncCompile();
  await FailSyncInstantiate();
}

assertPromiseResult(TestAll());
