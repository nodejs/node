// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --maglev

// A corrupted descriptor can also make Map::NextFreeFieldStorageLocation()
// report an IN OBJECT location. The length Maglev then computes stays
// positive, so the length CHECK does not catch it and only the is_in_object
// CHECK does.

const kHeapObjectTag = 1;
const kTaggedSize = 4;
// OffsetInWordsField is kDescriptorIndexBitCount + 1 bits wide and is
// immediately followed by the in-object bit.
const kOffsetInWordsBits = 11;

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = address => memory.getUint32(address, true);
const write32 = (address, value) => memory.setUint32(address, value, true);

// Four own properties exactly fill the in-object fields of this shape, so the
// next property added is the first out-of-object one and the extend path runs.
function makeShape() {
  const o = {};
  o.p0 = 0;
  o.p1 = 1;
  o.p2 = 2;
  o.p3 = 3;
  return o;
}

function grow(o, value) {
  o.extra = value;
}

%PrepareFunctionForOptimization(grow);
for (let i = 0; i < 20; i++) grow(makeShape(), i);

// Find the details word of each property at run time instead of hard-coding
// the Map, DescriptorArray and PropertyDetails layouts: in the descriptor
// array each key is followed by its details.
const map = read32(Sandbox.getAddressOf(makeShape())) - kHeapObjectTag;
const keys = ['p0', 'p1', 'p2', 'p3'].map(
    k => Sandbox.getAddressOf(k) + kHeapObjectTag);
function findDetails(descriptors) {
  const result = [];
  for (const key of keys) {
    for (let a = descriptors; a < descriptors + 64 * kTaggedSize;
         a += kTaggedSize) {
      if (read32(a) == key) {
        result.push(a + kTaggedSize);
        break;
      }
    }
  }
  return result.length == keys.length ? result : null;
}
let details = null;
for (let a = map + kTaggedSize; a < map + 16 * kTaggedSize && !details;
     a += kTaggedSize) {
  const candidate = read32(a);
  if ((candidate & kHeapObjectTag) == 0) continue;
  if (!Sandbox.isValidObjectAt(candidate - kHeapObjectTag)) continue;
  details = findDetails(candidate - kHeapObjectTag);
}
assertNotNull(details);

// The four fields sit in consecutive in-object words, so their details differ
// only in the offset field, by one word each.
const raw = details.map(read32);
const step = raw[1] - raw[0];
assertTrue(step > 0 && (step & (step - 1)) == 0);
assertEquals(step, raw[2] - raw[1]);
assertEquals(step, raw[3] - raw[2]);
const kOffsetShift = Math.log2(step);
const kOffsetMask = ((1 << kOffsetInWordsBits) - 1) << kOffsetShift;
const kIsInObjectBit = 1 << (kOffsetShift + kOffsetInWordsBits);
const lastDetails = details[3];
const last = raw[3];
// Fail loudly instead of corrupting an unrelated word if the layout moves.
assertTrue((last & kIsInObjectBit) != 0);

// Keep the in-object bit set and move the last field back to the first
// in-object word, so the next free location is reported as in object while
// the computed length stays positive.
const first = (raw[0] & kOffsetMask) >>> kOffsetShift;
write32(lastDetails,
        ((last & ~kOffsetMask) | (first << kOffsetShift)) >>> 0);
%OptimizeMaglevOnNextCall(grow);
grow(makeShape(), 0);
