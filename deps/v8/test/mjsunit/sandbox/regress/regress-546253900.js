// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --sandbox-testing

const CAGE_VIEW_SIZE = 0x100000000;
const memv = new DataView(new Sandbox.MemoryView(0, CAGE_VIEW_SIZE));

const u32 = (address) => memv.getUint32(address, true);
const w32 = (address, value) => memv.setUint32(address, value, true);
const u64 = (address) => memv.getBigUint64(address, true);

// JSArrayBuffer layout tail:
//   raw_byte_length_ (bounded size, 8) | raw_max_byte_length_ (8)
//   | backing_store_ (sandboxed ptr, 8) | extension_ (EPT handle, 4)
// Bounded sizes are stored shifted left by kBoundedSizeShift == 29.
function findArrayBufferExtensionOffset() {
  const length = 0x41410;
  const probe = new ArrayBuffer(length);
  const address = Sandbox.getAddressOf(probe);
  const size = Sandbox.getSizeOf(probe);
  const encodedLength = BigInt(length) << 29n;
  for (let offset = 0; offset + 8 <= size; offset += 4) {
    if (u64(address + offset) === encodedLength) return offset + 24;
  }
  throw new Error("could not locate JSArrayBuffer::extension_");
}

// WasmMemoryObject layout:
//   ... | array_buffer_ (tagged, 4) | managed_backing_store_ (tagged, 4) | ...
function findWasmMemoryFieldOffsets(memory) {
  const address = Sandbox.getAddressOf(memory);
  const size = Sandbox.getSizeOf(memory);
  const taggedBuffer = (Sandbox.getAddressOf(memory.buffer) + 1) >>> 0;
  for (let offset = 0; offset + 4 <= size; offset += 4) {
    if (u32(address + offset) === taggedBuffer) {
      return {
        arrayBuffer: offset,
        managedBackingStore: offset + 4,
      };
    }
  }
  throw new Error("could not locate WasmMemoryObject::array_buffer_");
}

const extensionOffset = findArrayBufferExtensionOffset();

// A is transported to the worker but deliberately left unregistered.
const memoryA =
    new WebAssembly.Memory({initial: 1, maximum: 16, shared: true});
// B is the genuine same-tag decoy selected by the substituted extension.
const memoryB =
    new WebAssembly.Memory({initial: 1, maximum: 16, shared: true});
// The keeper roots A's original CppGCManaged<BackingStore> wrapper while the
// DCHECK build temporarily points memoryA at memoryB's wrapper.
const keeper =
    new WebAssembly.Memory({initial: 1, maximum: 16, shared: true});

const sabA = memoryA.buffer;
const sabB = memoryB.buffer;

const fieldsA = findWasmMemoryFieldOffsets(memoryA);
const fieldsB = findWasmMemoryFieldOffsets(memoryB);
const fieldsKeeper = findWasmMemoryFieldOffsets(keeper);

const sabAAddress = Sandbox.getAddressOf(sabA);
const sabBAddress = Sandbox.getAddressOf(sabB);
const memoryAAddress = Sandbox.getAddressOf(memoryA);
const memoryBAddress = Sandbox.getAddressOf(memoryB);
const keeperAddress = Sandbox.getAddressOf(keeper);

const savedExtension = u32(sabAAddress + extensionOffset);
const savedManaged =
    u32(memoryAAddress + fieldsA.managedBackingStore);

function substituteBackingStoreB() {
  w32(sabAAddress + extensionOffset, u32(sabBAddress + extensionOffset));

  // These tagged writes satisfy the DCHECK in WasmMemoryObject::GetArrayBuffer()
  // for dcheck_always_on builds.
  w32(keeperAddress + fieldsKeeper.managedBackingStore, savedManaged);
  w32(memoryAAddress + fieldsA.managedBackingStore,
      u32(memoryBAddress + fieldsB.managedBackingStore));
}

function restoreBackingStoreA() {
  w32(sabAAddress + extensionOffset, savedExtension);
  w32(memoryAAddress + fieldsA.managedBackingStore,
      u32(keeperAddress + fieldsKeeper.managedBackingStore));
}

const workerSource = `
  onmessage = function(event) {
    postMessage("attached");
  };
`;

const worker = new Worker(workerSource, {type: "string"});

worker.postMessage({
  // The delegate sees sabA first and memoizes backing store A by object identity.
  a: sabA,
  get b() {
    // In-cage mutation: swaps extension handle to B during serialization.
    substituteBackingStoreB();
    return memoryA;
  },
});

worker.getMessage();

restoreBackingStoreA();

// On worker teardown, Purge() must find and remove the worker isolate from
// backing store A since it was registered upon deserialization.
worker.terminateAndWait();

// If backing store A was registered properly, this grow() call will not
// dereference a dangling worker Isolate pointer.
memoryA.grow(1);
