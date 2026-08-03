// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function addOf(obj)
{
    return Sandbox.getAddressOf(obj)
}

function write4(ptr, value)
{
    memory.setUint32(ptr, value, true);
}

function foo() {} // for WebAssembly.Suspending

var builder = new WasmModuleBuilder();
var struct1 = builder.addStruct([makeField(kWasmI64, true)]);

builder.addFunction('createStruct', makeSig([], [wasmRefType(struct1)])) // for testing
.addBody([
    kGCPrefix, kExprStructNewDefault, struct1
]).exportFunc();

builder.addFunction('write', makeSig([wasmRefType(struct1), kWasmI64], [])) // index 1
.addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kGCPrefix, kExprStructSet, struct1, 0
]).exportFunc()

builder.addFunction('dummy', makeSig([kWasmI64, kWasmI64], [])) // index 2
.addBody([
]).exportFunc();

var instance = builder.instantiate()
var exports = instance.exports;
var dummy = exports.dummy;
var createStruct = exports.createStruct;
var write = exports.write;

write(createStruct(), 0x1n);


var suspending = new WebAssembly.Suspending(foo)
// // change callable of WasmSuspendingObject
write4(addOf(suspending) + 0xc, addOf(write) + 1);


var builder2 = new WasmModuleBuilder();
struct1 = builder2.addStruct([makeField(kWasmI64, true)]);
var type = builder2.addType(makeSig([wasmRefType(struct1), kWasmI64], []));

for(let i = 0; i < 2; i++)
    builder2.addImport('test', 'write', type);
var importIdx = builder2.addImport('test', 'write', type); // use func type of index 2 (dummy)
builder2.addTable(kWasmFuncRef, 1, 1, [kExprRefFunc, importIdx]); // for function declared
builder2.addFunction('reExportOnlyInternal', makeSig([], [kWasmFuncRef]))
.addBody([
    kExprRefFunc, importIdx,
]).exportFunc();


var instance2 = builder2.instantiate({test: {write: suspending}});
var exports2 = instance2.exports;
var reExportOnlyInternal = exports2.reExportOnlyInternal;

var write64 = reExportOnlyInternal()

assertThrows(
    // write to 0x0000011111111111
    () => write64(0xdeadbeefn, 0x0000011111111111n - 7n), TypeError);
