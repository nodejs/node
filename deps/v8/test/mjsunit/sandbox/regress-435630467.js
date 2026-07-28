// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const kJSFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const kSharedFunctionInfoType = Sandbox.getInstanceTypeIdFor('SHARED_FUNCTION_INFO_TYPE');
const kJSFunctionSFIOffset = Sandbox.getFieldOffset(kJSFunctionType, 'shared_function_info');
const kSharedFunctionInfoTrustedFunctionDataOffset = Sandbox.getFieldOffset(kSharedFunctionInfoType, 'trusted_function_data');
const kSharedFunctionInfoUntrustedFunctionDataOffset = Sandbox.getFieldOffset(kSharedFunctionInfoType, 'function_data');
const kSharedFunctionInfoScriptOffset = Sandbox.getFieldOffset(kSharedFunctionInfoType, 'script');
const kHeapObjectTag = 1;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getBuiltinId(name) {
  return Sandbox.getBuiltinNames().indexOf(name);
}

function setFunctionsSFIToBuiltin(f, builtin_id) {
  let f_addr = Sandbox.getAddressOf(f);
  let sfi = memory.getUint32(f_addr + kJSFunctionSFIOffset, true) - kHeapObjectTag;
  // For builtins trusted_function_data should be zero and script should be
  // undefined.
  memory.setUint32(sfi + kSharedFunctionInfoTrustedFunctionDataOffset, 0, true);
  memory.setUint32(sfi + kSharedFunctionInfoUntrustedFunctionDataOffset, builtin_id << 1, true);
  memory.setUint32(sfi + kSharedFunctionInfoScriptOffset, 0x11, true);  // undefined
}

// Function with 0x100 args.
let fn = new Function(Array(0x100).fill().map((_, i)=>'p'+i), '');
%PrepareFunctionForOptimization(fn);
fn();

let wrap_fn = (run) => {
  run();
};


%PrepareFunctionForOptimization(wrap_fn);

Sandbox.setFunctionCodeToBuiltin(fn, getBuiltinId("DebugBreakTrampoline"));

setFunctionsSFIToBuiltin(fn, getBuiltinId("EmptyFunction"));

wrap_fn(fn);
// Arguments count mismatch must be detected during the previous call.
assertUnreachable();
