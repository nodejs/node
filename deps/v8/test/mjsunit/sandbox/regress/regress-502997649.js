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
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x06,                    // section length 6
  0x01, 0x60,              // types count 1:  kind: func
  0x01, 0x7f,              // param count 1:  i32
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $f (param i32) (result i32)

  0x07,                    // section kind: Export
  0x05,                    // section length 5
  0x01,                    // exports count 1: export # 0
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x09,                    // section length 9
  0x01,                    // functions count 1
                           // function #0 $f
  0x07,                    // body size 7
  0x00,                    // 0 entries in locals list
  0x20, 0x00,              // local.get $var0
  0x41, 0x01,              // i32.const 1
  0x6a,                    // i32.add
  0x0b,                    // end
]);

let mod = new WebAssembly.Module(kWasmBytes);
let inst = new WebAssembly.Instance(mod);
let f = inst.exports.f;

function g() { f(); }
%PrepareFunctionForOptimization(g);

%BlockAt('TurbofanEarlyGraphTrimming', 10000);
%OptimizeFunctionOnNextCall(g, 'concurrent');
g();

assertTrue(%WaitUntilBlocked('TurbofanEarlyGraphTrimming', 10000));

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

assertTrue(%Resume('TurbofanEarlyGraphTrimming'));

%WaitForBackgroundOptimization();
