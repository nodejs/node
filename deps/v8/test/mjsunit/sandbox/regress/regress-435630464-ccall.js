// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kBuiltins = Sandbox.getBuiltinNames();

const kJSFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const kSharedFunctionInfoType = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');
const kJSFunctionSFIOffset = Sandbox.getFieldOffset(kJSFunctionType, 'shared_function_info');
const kSharedFunctionInfoFormalParameterCountOffset = Sandbox.getFieldOffset(kSharedFunctionInfoType, 'formal_parameter_count');
const kHeapObjectTag = 1;
const kDontAdaptArgumentsSentinel = 0;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getFormalParameterCount(f) {
  let f_addr = Sandbox.getAddressOf(f);
  let sfi_addr = memory.getUint32(f_addr + kJSFunctionSFIOffset, true) - kHeapObjectTag;
  return memory.getUint16(sfi_addr + kSharedFunctionInfoFormalParameterCountOffset, true);
}

var f = BigInt;

// The test requires a builtin with kDontAdaptArgumentsSentinel!
assertEquals(getFormalParameterCount(f), kDontAdaptArgumentsSentinel);

// Attempt to install such a builtin into function must safely crash.
Sandbox.setFunctionCodeToBuiltin(f, kBuiltins.indexOf("MemMove"));
assertUnreachable();
