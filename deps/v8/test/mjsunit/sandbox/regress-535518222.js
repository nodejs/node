// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// DrumBrake's nested interpreted Wasm-to-Wasm call path selects the callee's
// interpreter object, runtime, module and signature by reloading the in-cage
// WasmInstanceObject::trusted_data handle of the target instance. A sandbox
// attacker can same-tag swap that handle to point at a different instance's
// trusted data. Unlike the direct JS->interpreter entry
// (Runtime_WasmRunInterpreter, see regress-522294322), the nested path went
// through WasmTrustedInstanceData::Get{OrCreate,}InterpreterObject, which did
// not re-validate that the reloaded trusted data still belongs to the target
// instance. The swap therefore ran instance B's function under B's runtime
// while the caller marshalled the shared result slots using instance A's
// callsite signature, returning B's externref (a native DirectHandle word) to
// JavaScript as an i64 and disclosing a native, out-of-cage address.

// Callee instance A returns a single i64.
const builderA = new WasmModuleBuilder();
builderA.addFunction('f0', makeSig([], [kWasmI64]))
    .addBody([...wasmI64Const(0)])
    .exportFunc();
const instanceA = builderA.instantiate();

// Donor instance B returns an externref at the same function index.
const builderB = new WasmModuleBuilder();
builderB.addFunction('f0', makeSig([], [kWasmExternRef]))
    .addBody([kExprRefNull, kExternRefCode])
    .exportFunc();
const instanceB = builderB.instantiate();

// Caller instance imports A's f0 with an i64-returning callsite signature and
// calls it, which drives the interpreter's cross-instance external-call path
// (WasmInterpreterRuntime::CallExternalWasmFunction).
const builderCaller = new WasmModuleBuilder();
const sig = builderCaller.addType(makeSig([], [kWasmI64]));
const imported = builderCaller.addImport('m', 'f', sig);
builderCaller.addFunction('call', sig)
    .addBody([kExprCallFunction, imported])
    .exportFunc();
const caller = builderCaller.instantiate({m: {f: instanceA.exports.f0}});

// Warm up A's and B's interpreters so their interpreter objects/handles exist.
instanceA.exports.f0();
instanceB.exports.f0();

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

// The cross-instance external call now reloads A's swapped trusted_data (= B)
// to pick the interpreter object/runtime. This must be detected as a sandbox
// violation and kill the process rather than returning B's externref result to
// JavaScript as an i64 native-address disclosure.
caller.exports.call();

assertUnreachable("Process should have been killed.");
