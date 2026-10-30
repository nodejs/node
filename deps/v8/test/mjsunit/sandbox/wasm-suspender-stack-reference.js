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
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x07,                    // section length 7
  0x02,                    // types count 2
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x02,                    // section kind: Import
  0x07,                    // section length 7
  0x01, 0x01,              // imports count 1: module name length:  1
  0x6d,                    // module name: m
  0x01,                    // field name length:  1
  0x70,                    // field name: p
  0x00, 0x00,              // kind or compact flag: function
                           // import #0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x01,              // functions count 1: 1 $f

  0x07,                    // section kind: Export
  0x05,                    // section length 5
  0x01,                    // exports count 1: export # 0
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x01,              // kind: function index:  1

  0x0a,                    // section kind: Code
  0x06,                    // section length 6
  0x01,                    // functions count 1
                           // function #1 $f
  0x04,                    // body size 4
  0x00,                    // 0 entries in locals list
  0x10, 0x00,              // call $m.p
  0x0b,                    // end
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
