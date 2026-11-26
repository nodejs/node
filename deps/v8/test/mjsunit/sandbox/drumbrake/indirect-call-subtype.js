// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless --expose-memory-corruption-api

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// DrumBrake regression test: call_indirect checks subtyping with the trusted
// type canonicalizer rather than the Map / WasmTypeInfo supertypes chain.

// Two unrelated non-final signatures (no declared subtype relationship).
const $sig_i_l = kSig_i_l;   // (i64) -> i32
const $sig_l_l = kSig_l_l;   // (i64) -> i64

const builder = new WasmModuleBuilder();
const $tSigA = builder.addType($sig_i_l, kNoSuperType, false);
const $tSigB = builder.addType($sig_l_l, kNoSuperType, false);

const $funcA = builder.addFunction('funcA', $tSigA)
    .addBody([...wasmI32Const(0)])
    .exportFunc();
const $funcB = builder.addFunction('funcB', $tSigB)
    .addBody([kExprLocalGet, 0])
    .exportFunc();

const $table = builder.addTable(
    kWasmAnyFunc, 1, 1, [kExprRefFunc, ...wasmSignedLeb($funcB.index)]);

// Caller performs call_indirect against the (non-final) sig of funcA, but the
// only table entry actually holds funcB, whose unrelated signature is sig_l_l.
builder.addFunction('call', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprI64UConvertI32,
      ...wasmI32Const(0),            // table index
      kExprCallIndirect, $tSigA, $table.index,
    ])
    .exportFunc();

const instance = builder.instantiate();
const call = instance.exports.call;

// Sanity: without any corruption, the mismatched signature must trap.
assertTraps(kTrapFuncSigMismatch, () => call(1));

// --- Corrupt the in-sandbox Map/WasmTypeInfo supertypes chain --------------
const kHeapObjectTag = 1;
const kWasmGlobalObjectBufferOffset = 0x10;
const kFixedArrayElement0Offset = 0x8;
const kMapOffset = 0;
const kFuncRefMapTypeInfoOffset = 0x14;
const kTypeInfoSupertypesOffset = 0x10;
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

function extract_wasmglobal_value(global) {
  let pbuf = getField(getPtr(global), kWasmGlobalObjectBufferOffset);
  let pval = getField(pbuf, kFixedArrayElement0Offset);
  return pval;
}

// Forge `funcB`'s declared supertype to be `funcA`'s signature, i.e. make the
// sandbox-resident type info of sig_l_l claim sig_i_l as its (only) supertype,
// even though the module never declared any such relationship.
function forge_supertype(sub_fn, super_fn) {
  let g = new WebAssembly.Global({value: 'anyfunc', mutable: true});

  g.value = sub_fn;
  let funcref_sub = extract_wasmglobal_value(g);                    // WASM_FUNC_REF_TYPE
  let map_sub = getField(funcref_sub, kMapOffset);                  // Map of WASM_FUNC_REF_TYPE
  let typeinfo_sub = getField(map_sub, kFuncRefMapTypeInfoOffset);  // WASM_TYPE_INFO_TYPE

  g.value = super_fn;
  let funcref_sup = extract_wasmglobal_value(g);
  let map_sup = getField(funcref_sup, kMapOffset);

  // typeinfo_sub.supertypes[0] = map_sup
  setField(typeinfo_sub, kTypeInfoSupertypesOffset, map_sup);
}

forge_supertype(instance.exports.funcB, instance.exports.funcA);

// Even with the Map/WasmTypeInfo chain corrupted to forge sig_l_l <: sig_i_l,
// the trusted type canonicalizer must still reject the call: the fix does not
// consult the corrupted data.
assertTraps(kTrapFuncSigMismatch, () => call(1));
