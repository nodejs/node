// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --wasm-wasmfx --expose-gc --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_index = builder.addCont(sig_v_v);
let box_type = builder.addStruct([makeField(wasmRefType(cont_index), true)]);

let nop = builder.addFunction('nop', kSig_v_v).addBody([]).exportFunc();

builder.addFunction('make_cont', makeSig([], [wasmRefType(box_type)]))
    .addBody([
      kExprRefFunc, nop.index,
      kExprContNew, cont_index,
      kGCPrefix, kExprStructNew, box_type,
    ]).exportFunc();

builder.addFunction('resume_cont', makeSig([wasmRefType(box_type)], []))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, box_type, 0,
      kExprResume, cont_index, 0,
    ]).exportFunc();

let instance = builder.instantiate();

let boxA = instance.exports.make_cont();
let boxB = instance.exports.make_cont();

const kHeapObjectTag = 1;
const kTaggedSize = 4;
const kStructField0Offset = 2 * kTaggedSize;
const kContStackObjOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('WASM_CONTINUATION_OBJECT_TYPE'), 'stack_obj');
const kStackObjStackOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('WASM_STACK_OBJECT_TYPE'), 'stack');

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getField(ptr, offset) {
  return memory.getUint32(ptr + offset - kHeapObjectTag, true);
}
function setField(ptr, offset, value) {
  memory.setUint32(ptr + offset - kHeapObjectTag, value, true);
}

let contA = getField(getPtr(boxA), kStructField0Offset);
let contB = getField(getPtr(boxB), kStructField0Offset);

let stackObjA = getField(contA, kContStackObjOffset);
let stackObjB = getField(contB, kContStackObjOffset);

let handleA = getField(stackObjA, kStackObjStackOffset);
let handleB = getField(stackObjB, kStackObjStackOffset);

// Swap the EPT handles between stackObjA and stackObjB, and point contA to
// stackObjB (which now holds handleA). StackMemory_A's out-of-cage stack_obj_
// reference still points to stackObjA.
setField(stackObjA, kStackObjStackOffset, handleB);
setField(stackObjB, kStackObjStackOffset, handleA);
setField(contA, kContStackObjOffset, stackObjB);

// Resume contA so StackMemory_A completes and retires to the stack pool.
instance.exports.resume_cont(boxA);

// Free finished stacks in the stack pool.
gc({type: 'major', execution: 'sync', flavor: 'last-resort'});

// Re-read stackObjB's address after GC and print it; if handleA was not zapped
// on retirement, this dereferences the freed StackMemory_A.
contB = getField(getPtr(boxB), kStructField0Offset);
stackObjB = getField(contB, kContStackObjOffset);
%DebugPrint(Sandbox.getObjectAt(stackObjB));
