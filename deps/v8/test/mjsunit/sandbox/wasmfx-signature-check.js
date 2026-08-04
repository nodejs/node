// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --experimental-wasm-wasmfx --expose-memory-corruption-api

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let sigA = builder.addType(makeSig([kWasmI64], []));
let contA = builder.addCont(sigA);
let boxA = builder.addStruct([makeField(wasmRefType(contA), true)]);

let boxC = builder.addStruct([makeField(kWasmI32, true)]);
let sigB = builder.addType(makeSig([wasmRefType(boxC)], [kWasmI32]));
let contB = builder.addCont(sigB);
let boxB = builder.addStruct([makeField(wasmRefType(contB), true)]);

let funcA = builder.addFunction("funcA", sigA).addBody([]).exportFunc();
let funcB = builder.addFunction("funcB", sigB)
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, boxC, 0,
  ]).exportFunc();

builder.addFunction("make_contA", makeSig([], [wasmRefType(boxA)]))
  .addBody([
    kExprRefFunc, funcA.index,
    kExprContNew, contA,
    kGCPrefix, kExprStructNew, boxA,
  ]).exportFunc();

builder.addFunction("make_contB", makeSig([], [wasmRefType(boxB)]))
  .addBody([
    kExprRefFunc, funcB.index,
    kExprContNew, contB,
    kGCPrefix, kExprStructNew, boxB,
  ]).exportFunc();

builder.addFunction("resume_contA", makeSig([wasmRefType(boxA), kWasmI64], []))
  .addBody([
    kExprLocalGet, 1,
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, boxA, 0,
    kExprResume, contA, 0,
  ]).exportFunc();

let instance = builder.instantiate();

let objA = instance.exports.make_contA();
let objB = instance.exports.make_contB();

const kHeapObjectTag = 1;
const kTaggedSize = 4;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

let addrA = getPtr(objA);
let addrB = getPtr(objB);

let contB_ptr = getField(addrB, 2 * kTaggedSize);

// Overwrite objA's field 0 (contA) with contB.
setField(addrA, 2 * kTaggedSize, contB_ptr);

// Now resuming contA should trigger the signature check trap.
assertThrows(
    () => { instance.exports.resume_contA(objA, 0x414141414141n); },
    WebAssembly.RuntimeError,
    /function signature mismatch/
);
