// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --turbolev

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kJSFunctionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const kDispatchHandleOffset =
    Sandbox.getFieldOffset(kJSFunctionType, 'dispatch_handle');

const builder = new WasmModuleBuilder();
builder.addFunction('w0', kSig_v_i)
  .addBody([])
  .exportFunc();
const instance = builder.instantiate();
const w0 = instance.exports.w0;

function caller(x) {
  w0(x);
}

%PrepareFunctionForOptimization(caller);
caller(0);

const math_sin_addr = Sandbox.getAddressOf(Math.sin);
const math_sin_handle =
    memory.getUint32(math_sin_addr + kDispatchHandleOffset, true);

const w0_addr = Sandbox.getAddressOf(w0);
memory.setUint32(w0_addr + kDispatchHandleOffset, math_sin_handle, true);

%OptimizeFunctionOnNextCall(caller);
caller(0);
