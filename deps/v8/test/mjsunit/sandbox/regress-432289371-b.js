// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function addrOf(obj)
{
    return Sandbox.getAddressOf(obj)
}

function read4(ptr)
{
    return memory.getUint32(ptr, true);
}

function write4(ptr, value)
{
    memory.setUint32(ptr, value, true);
}


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




var builder2 = new WasmModuleBuilder();
struct1 = builder2.addStruct([makeField(kWasmI64, true)]);
var structFuncRef = builder2.addStruct([makeField(kWasmFuncRef, true)]); // for leak func_ref
var type = builder2.addType(makeSig([wasmRefType(struct1), kWasmI64], []));

for(let i = 0; i < 2; i++)
    builder2.addImport('test', 'write', type);
var importIdx = builder2.addImport('test', 'write', type); // use func type of index 2 (dummy)
builder2.addTable(kWasmFuncRef, 1, 1, [kExprRefFunc, importIdx]); // for function declared
builder2.addFunction('reExportOnlyInternal', makeSig([], [kWasmFuncRef]))
.addBody([
    kExprRefFunc, importIdx,
]).exportFunc();
builder2.addFunction('returnStructFuncRef', makeSig([], [wasmRefType(structFuncRef)])) // leak func_ref
.addBody([
    kExprRefFunc, importIdx,
    kGCPrefix, kExprStructNew, structFuncRef,
]).exportFunc()

var arr = []
var FIXED_ARRAY_MAP = read4(read4(addrOf(arr) + 0x8) - 1);

var instance2 = builder2.instantiate({test: {write: write}});
var exports2 = instance2.exports;
var reExportOnlyInternal = exports2.reExportOnlyInternal;
var returnStructFuncRef = exports2.returnStructFuncRef;
var structRef = returnStructFuncRef();

// modify func_refs array
var func_ref = read4(addrOf(structRef) + 0x8);
var startAddr = addrOf(exports2) - 0x5000
var endAddr = startAddr + 0x5000;
for(let i = startAddr; i < endAddr; i += 4) {
    if(read4(i) != FIXED_ARRAY_MAP) continue;
    if(read4(i + 4) != (5 << 1)) continue;
    if(read4(i + 8 + importIdx*4) == func_ref) {
        // print('Found the function at: ' + i.toString(16));
        write4(i + 8 + importIdx*4, 0);
        break;
    }
}

var write64 = reExportOnlyInternal()

assertThrows(
    // write to 0x0000011111111111
    () => write64(0xdeadbeefn, 0x0000011111111111n - 7n), TypeError);
