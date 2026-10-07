// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const getField = (obj, offset) => memory.getUint32(obj + offset, true);
const setField = (obj, offset, value) =>
    memory.setUint32(obj + offset, value, true);

const kTrustedPointerHandleShift =
    Sandbox.getMetadata().trustedPointerHandleShift;
const kTrustedPointerHandleStride = 1 << kTrustedPointerHandleShift;

const builder = new WasmModuleBuilder();
const structType = builder.addStruct([
  makeField(kWasmI64, true),
  makeField(kWasmI64, true),
  makeField(kWasmI64, true),
]);
const arrType = builder.addArray(kWasmI32);
const sig = builder.addType(kSig_v_v);

const importedLen = builder.addImportedGlobal('m', 'len', kWasmI32, false);
const target = builder.addFunction('target', sig).addBody([
  kExprGlobalGet, 3,
  ...wasmI64Const(0x41414141),
  kGCPrefix, kExprStructSet, structType, 2,
]).exportFunc();

// Global 1 evaluates (ref.func $target), allocating a WasmInternalFunction
// whose protected_implicit_arg points to the new WasmTrustedInstanceData.
builder.addGlobal(wasmRefType(sig), false, false, [
  kExprRefFunc, target.index,
]);
// Global 2 traps when importedLen == 0x7fffffff, aborting InitGlobals before
// Global 3 (a non-nullable struct reference) is initialized.
builder.addGlobal(wasmRefType(arrType), false, false, [
  kExprGlobalGet, importedLen,
  kGCPrefix, kExprArrayNewDefault, arrType,
]);
builder.addGlobal(wasmRefType(structType), false, false, [
  kGCPrefix, kExprStructNewDefault, structType,
]);

const table = builder.addTable(wasmRefNullType(sig), 1, 1).exportAs('table');
builder.addActiveElementSegment(
    table.index, wasmI32Const(0), [[kExprRefFunc, target.index]],
    wasmRefNullType(sig));
builder.addFunction('invoker', sig).addBody([
  kExprI32Const, 0,
  kExprTableGet, table.index,
  kExprCallRef, sig,
]).exportFunc();

const module = builder.toModule();
const good = new WebAssembly.Instance(module, {m: {len: 0}});
const tableAddr = Sandbox.getAddressOf(good.exports.table);
const entriesOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdOf(good.exports.table), 'entries');
const entriesAddr = getField(tableAddr, entriesOffset) - 1;
const funcRefAddr = getField(entriesAddr, 8) - 1;
const trustedInternalOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdOfObjectAt(funcRefAddr), 'trusted_internal');
const beforeHandle = getField(funcRefAddr, trustedInternalOffset);

assertThrows(
    () => new WebAssembly.Instance(module, {m: {len: 0x7fffffff}}),
    WebAssembly.RuntimeError);

const after = new WebAssembly.Instance(module, {m: {len: 0}});
const afterEntriesAddr =
    getField(Sandbox.getAddressOf(after.exports.table), entriesOffset) - 1;
const afterFuncRefAddr = getField(afterEntriesAddr, 8) - 1;
const afterHandle = getField(afterFuncRefAddr, trustedInternalOffset);

// Target the WasmInternalFunction handle allocated during the failed instance's
// InitGlobals (the 3rd trusted handle allocated per instantiation, after
// WasmTrustedInstanceData and WasmDispatchTable).
const failedInternalFuncHandle =
    afterHandle - 3 * kTrustedPointerHandleStride;
assertTrue(failedInternalFuncHandle > beforeHandle);
setField(funcRefAddr, trustedInternalOffset, failedInternalFuncHandle);
good.exports.invoker();
