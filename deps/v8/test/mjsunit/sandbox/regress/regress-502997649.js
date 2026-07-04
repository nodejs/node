// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --sandbox-testing

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const ptr = (c) => c & ~1;

const JS_FUNCTION_TYPE = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const SHARED_FUNCTION_INFO_TYPE = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');
const WASM_INSTANCE_OBJECT_TYPE = Sandbox.getInstanceTypeIdFor('WASM_INSTANCE_OBJECT_TYPE');
const WASM_MODULE_OBJECT_TYPE = Sandbox.getInstanceTypeIdFor('WASM_MODULE_OBJECT_TYPE');
const SCRIPT_TYPE = Sandbox.getInstanceTypeIdFor('SCRIPT_TYPE');

const kWasmBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01,
  0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x05, 0x01, 0x01, 0x66, 0x00,
  0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x0b
]);

let mod = new WebAssembly.Module(kWasmBytes);
let inst = new WebAssembly.Instance(mod);
let f = inst.exports.f;

function g() { f(); }
%PrepareFunctionForOptimization(g);

%BlockAt('TurbofanEarlyGraphTrimming', 10000);
%OptimizeFunctionOnNextCall(g, 'concurrent');
g();

%WaitUntilBlocked('TurbofanEarlyGraphTrimming', 10000);

const aMod = Sandbox.getAddressOf(mod);
const aInst = Sandbox.getAddressOf(inst);
const aF = Sandbox.getAddressOf(f);
const aSFI = ptr(mem.getUint32(aF + Sandbox.getFieldOffset(JS_FUNCTION_TYPE, 'shared_function_info'), true));
const aScript = ptr(mem.getUint32(aSFI + Sandbox.getFieldOffset(SHARED_FUNCTION_INFO_TYPE, 'script'), true));

mem.setUint32(aSFI + Sandbox.getFieldOffset(SHARED_FUNCTION_INFO_TYPE, 'trusted_function_data'), 0, true);
mem.setUint32(aInst + Sandbox.getFieldOffset(WASM_INSTANCE_OBJECT_TYPE, 'module_object') - 4, 0, true);
mem.setUint32(aMod + Sandbox.getFieldOffset(WASM_MODULE_OBJECT_TYPE, 'managed_native_module'), 0, true);
mem.setUint32(aScript + Sandbox.getFieldOffset(SCRIPT_TYPE, 'wasm_managed_native_module'), 0, true);

%ClearFunctionFeedback(g);

f = null;
mod = null;
inst = null;

let junk = [];
for (let i = 0; i < 10000; i++) junk.push(new Array(10).fill(1));
junk = null;

gc(); gc(); gc();

%Resume('TurbofanEarlyGraphTrimming');

%WaitForBackgroundOptimization();
