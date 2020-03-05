// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

const Debug = new DebugWrapper();
Debug.enable();

// Record the ID of the first script reported. This is to ignore
// the (now deprecated) fake scripts that are generated for every
// Wasm module.
let scriptId;
Debug.setListener((eventType, execState, eventData, data) => {
    assertEquals(Debug.DebugEvent.AfterCompile, eventType);
    if (scriptId === undefined) scriptId = eventData.scriptId;
});

// Create a simple Wasm script, which will be caught by the event listener.
const builder = new WasmModuleBuilder();
builder.addFunction('sub', kSig_i_ii)
// input is 2 args of type int and output is int
.addBody([
  kExprLocalGet, 0, // local.get i0
  kExprLocalGet, 1, // local.get i1
  kExprI32Sub]) // i32.sub i0 i1
.exportFunc();
const instance = builder.instantiate();

// By now we should have recorded the ID of the Wasm script above.
assertNotEquals(undefined, scriptId);

// Disable and re-enable the Debugger and collect the reported
// script IDs.
const scriptIds = new Set();
Debug.disable();
Debug.setListener((eventType, execState, eventData, data) => {
    assertEquals(Debug.DebugEvent.AfterCompile, eventType);
    scriptIds.add(eventData.scriptId);
});
Debug.enable();

// Make sure the Wasm script was reported.
assertTrue(scriptIds.has(scriptId));
