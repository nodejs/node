// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// The JS->Wasm wrapper sizes the on-stack argument and return buffers from the
// trusted (out-of-cage) signature of the called function, but reaches
// Runtime_WasmRunInterpreter through the in-cage WasmInstanceObject. A
// sandbox attacker can same-tag swap WasmInstanceObject::trusted_data_ to point
// at another instance's trusted data, so the runtime would read a mismatched
// signature and read/write its values past the fixed-size native-stack buffers.

// Module A exports a function with a single i64 return value. The interpreter
// wrapper therefore sizes its return buffer for one return value.
const builderA = new WasmModuleBuilder();
builderA.addFunction('f0', makeSig([], [kWasmI64]))
    .addBody([...wasmI64Const(0)])
    .exportFunc();
const instanceA = builderA.instantiate();

// Module B exports a function at the same index with many i64 return values.
const kNumReturns = 8;
const builderB = new WasmModuleBuilder();
const returns = new Array(kNumReturns).fill(kWasmI64);
let bodyB = [];
for (let i = 0; i < kNumReturns; ++i) {
  bodyB = bodyB.concat(wasmI64Const(0x4142434445464748n));
}
builderB.addFunction('f0', makeSig([], returns)).addBody(bodyB).exportFunc();
const instanceB = builderB.instantiate();

// Sandbox memory-corruption helpers.
const kHeapObjectTag = 1;
const kWasmInstanceObjectType =
    Sandbox.getInstanceTypeIdFor('WASM_INSTANCE_OBJECT_TYPE');
const kTrustedDataOffset =
    Sandbox.getFieldOffset(kWasmInstanceObjectType, 'trusted_data');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

// Same-tag swap: point instanceA's trusted_data indirect-pointer handle at
// instanceB's trusted data. Both handles carry the same trusted-pointer tag,
// so the trusted-pointer table tag check does not catch the swap.
const instA = getPtr(instanceA);
const instB = getPtr(instanceB);
setField(instA, kTrustedDataOffset, getField(instB, kTrustedDataOffset));

// Calling A's single-return export now resolves the swapped (module B's)
// signature inside Runtime_WasmRunInterpreter. This must be detected as a
// sandbox violation and kill the process rather than writing module B's return
// values past module A's return buffer on the native stack.
instanceA.exports.f0();

assertUnreachable("Process should have been killed.");
