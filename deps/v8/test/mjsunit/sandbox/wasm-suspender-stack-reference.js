// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const kHeapObjectTag = 1;
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = p => memory.getUint32(p, true);
const write32 = (p, value) => memory.setUint32(p, value, true);
const address = object => Sandbox.getAddressOf(object);
const bytes = new Uint8Array([
  0,97,115,109,1,0,0,0,1,7,2,96,0,0,96,0,0,2,7,1,1,109,1,112,0,0,
  3,2,1,1,7,5,1,1,102,0,1,10,6,1,4,0,16,0,11
]);
const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {
  m: {p: new WebAssembly.Suspending(() => new Promise(() => {}))}
});
const suspend = WebAssembly.promising(instance.exports.f);
const buffer1 = new Uint8Array(1);
const buffer2 = new Uint8Array(1);
const tagObj = new WebAssembly.Tag({parameters: []});
gc();

const extensionOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE'), 'extension');
const firstHandle = read32(address(buffer1.buffer) + extensionOffset);
const secondHandle = read32(address(buffer2.buffer) + extensionOffset);
const handleStride = secondHandle - firstHandle;
assertTrue(handleStride > 0);
suspend();

const mapType = Sandbox.getInstanceTypeIdFor('MAP_TYPE');
const instanceTypeOffset = Sandbox.getFieldOffset(mapType, 'instance_type');
const stackType = Sandbox.getInstanceTypeIdFor('WASM_STACK_OBJECT_TYPE');
const stackOffset = Sandbox.getFieldOffset(stackType, 'stack');
const tagOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('WASM_TAG_OBJECT_TYPE'), 'tag');

const p = read32(address(tagObj) + tagOffset) - kHeapObjectTag;
const metaMap = read32(read32(p) - kHeapObjectTag);
let stackMap = 0;
for (let addr = 0; addr < 0x10000; addr += 4) {
  if (read32(addr) === metaMap &&
      memory.getUint16(addr + instanceTypeOffset, true) === stackType) {
    stackMap = addr + kHeapObjectTag;
    break;
  }
}
assertNotEquals(0, stackMap);
write32(p, stackMap);
write32(p + stackOffset, secondHandle + handleStride);

// An in-cage stack object cannot acquire a trusted suspender's stack reference.
gc();
assertUnreachable();
