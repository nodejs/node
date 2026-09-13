// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper --expose-memory-corruption-api
// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kHeapObjectTag = 1;
const kJSRegExpDataOffset = Sandbox.getFieldOffset(
  Sandbox.getInstanceTypeIdFor('JS_REG_EXP_TYPE'), 'data');
const kJSFunctionSharedOffset = Sandbox.getFieldOffset(
  Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE'), 'shared_function_info');
const kSharedFunctionInfoWasmFunctionDataOffset = Sandbox.getFieldOffset(
  Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE'),
  'trusted_function_data');

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

let builder = new WasmModuleBuilder();

let type_i_i = builder.addType(kSig_i_i);

builder.addFunction("inc", type_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
    .exportFunc();

let wasm = builder.instantiate().exports;

let func = wasm.inc;

let re = /corruptor/g;
let reAddr = Sandbox.getAddressOf(re);
let regExpDataHandle = memory.getUint32(reAddr + kJSRegExpDataOffset, true);

let funcAddr = Sandbox.getAddressOf(func);
const kCageBase = funcAddr & 0xffffffff00000000;
let sfiAddr =
    (memory.getUint32(funcAddr + kJSFunctionSharedOffset, true) &
     ~kHeapObjectTag) |
    kCageBase;

memory.setUint32(sfiAddr + kSharedFunctionInfoWasmFunctionDataOffset,
                 regExpDataHandle, true);

// Should segfault.
func(42);

// CHECK: Caught harmless memory access violation
// CHECK-NOT: Unreachable
print("Unreachable");
