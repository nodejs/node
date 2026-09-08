// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const moduleABytes = new Uint8Array([
    0,   97,  115, 109, 1,   0,   0,   0,   1,   22,  5,   96,  0,   1,   127,
    96,  1,   111, 0,   96,  0,   1,   111, 96,  1,   127, 1,   127, 96,  0,
    1,   127, 2,   27,  2,   1,   109, 9,   102, 117, 110, 99,  116, 105, 111,
    110, 115, 1,   112, 1,   1,   1,   1,   109, 3,   116, 97,  103, 4,   0,
    1,   3,   3,   2,   3,   4,   4,   5,   1,   105, 1,   1,   1,   6,   32,
    6,   127, 1,   65,  239, 0,   11,  127, 1,   65,  0,   11,  127, 1,   65,
    0,   11,  127, 1,   65,  0,   11,  127, 1,   65,  0,   11,  127, 1,   65,
    0,   11,  7,   30,  3,   10,  101, 120, 99,  101, 112, 116, 105, 111, 110,
    115, 1,   1,   6,   100, 114, 105, 118, 101, 114, 0,   0,   4,   115, 101,
    101, 100, 0,   1,   10,  48,  2,   41,  0,   65,  0,   17,  0,   0,   26,
    32,  0,   4,   64,  2,   2,   31,  2,   1,   0,   0,   0,   65,  0,   37,
    1,   10,  11,  11,  26,  65,  0,   36,  5,   65,  0,   17,  0,   0,   15,
    11,  65,  0,   11,  4,   0,   65,  7,   11,  0,   22,  4,   110, 97,  109,
    101, 1,   15,  2,   0,   6,   100, 114, 105, 118, 101, 114, 1,   4,   115,
    101, 101, 100,
]);
const moduleDBytes = new Uint8Array([
    0,  97, 115, 109, 1,  0,   0,   0,   1,   13,  3,  96,  0,   1,   127,
    96, 0,  1,   127, 96, 0,   1,   127, 3,   4,   3,  0,   1,   2,   7,
    10, 1,  6,   116, 97, 114, 103, 101, 116, 0,   2,  10,  17,  3,   4,
    0,  65, 0,   11,  4,  0,   65,  1,   11,  5,   0,  65,  222, 1,   11,
    0,  28, 4,   110, 97, 109, 101, 1,   21,  3,   0,  4,   112, 97,  100,
    48, 1,  4,   112, 97, 100, 49,  2,   6,   116, 97, 114, 103, 101, 116
]);

const kHeapObjectTag = 1;
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = address => memory.getUint32(address, true);
const write32 = (address, val) => memory.setUint32(address, val >>> 0, true);
const tagged = object => (Sandbox.getAddressOf(object) + kHeapObjectTag) >>> 0;

function instanceTypeAt(address) {
  try {
    return Sandbox.getInstanceTypeOfObjectAt(address);
  } catch (_) {
    return undefined;
  }
}

function descriptorArrayOf(object) {
  const map = read32(Sandbox.getAddressOf(object)) & ~kHeapObjectTag;
  const size = Sandbox.getSizeOfObjectAt(map);
  for (let offset = 0; offset < size; offset += 4) {
    const candidate = read32(map + offset) & ~kHeapObjectTag;
    if (instanceTypeAt(candidate) === 'DESCRIPTOR_ARRAY_TYPE') return candidate;
  }
  throw new Error('descriptor array not found');
}

function findTaggedWord(objectAddress, expectedType, expectedValue) {
  const size = Sandbox.getSizeOfObjectAt(objectAddress);
  for (let offset = 0; offset < size; offset += 4) {
    const value = read32(objectAddress + offset);
    if (expectedValue !== undefined && value !== expectedValue) continue;
    if (instanceTypeAt(value & ~kHeapObjectTag) === expectedType) {
      return {offset, value};
    }
  }
  throw new Error(`tagged ${expectedType} word not found`);
}

const table =
    new WebAssembly.Table({element: 'anyfunc', initial: 1, maximum: 1});
const d = new WebAssembly.Instance(new WebAssembly.Module(moduleDBytes));
const a = new WebAssembly.Instance(new WebAssembly.Module(moduleABytes), {
  m: {functions: table, tag: WebAssembly.JSTag},
});
if (d.exports.target() !== 222) throw new Error('D target sanity failed');

// Recover the private exception-tag symbol from a genuine exception.
const genuine =
    new WebAssembly.Exception(new WebAssembly.Tag({parameters: []}), []);
const genuineDescriptors = descriptorArrayOf(genuine);
const hiddenTagSymbol = findTaggedWord(genuineDescriptors, 'SYMBOL_TYPE').value;

let getterCount = 0;
const forged = Object.defineProperty({}, 'x', {
  get() {
    ++getterCount;
    table.set(0, d.exports.target);
    return undefined;
  },
  configurable: true,
});

// Replace only the one-property donor descriptor's key. Its accessor details
// and AccessorPair remain unchanged.
const forgedDescriptors = descriptorArrayOf(forged);
const xName = Object.getOwnPropertyNames(forged)[0];
const descriptorKey = findTaggedWord(
    forgedDescriptors,
    Sandbox.getInstanceTypeOfObjectAt(Sandbox.getAddressOf(xName)),
    tagged(xName));
write32(forgedDescriptors + descriptorKey.offset, hiddenTagSymbol);

// Put the ordinary JSReceiver into the backing FixedArray of the exported
// Wasm-declared exnref table.
const tableType = Sandbox.getInstanceTypeIdFor('WASM_TABLE_OBJECT_TYPE');
const entriesOffset = Sandbox.getFieldOffset(tableType, 'entries');
const fixedArrayType = Sandbox.getInstanceTypeIdFor('FIXED_ARRAY_TYPE');
const fixedArrayDataOffset = Sandbox.getFieldOffset(fixedArrayType, 'data');
const exnTableAddress = Sandbox.getAddressOf(a.exports.exceptions);
const entries = read32(exnTableAddress + entriesOffset) & ~kHeapObjectTag;
write32(entries + fixedArrayDataOffset, tagged(forged));

// Only the first call_indirect site receives feedback and is inlined. The
// second site remains uninitialized and therefore generic.
table.set(0, a.exports.seed);
for (let i = 0; i < 20; ++i) {
  if (a.exports.driver(0) !== 0) throw new Error('training failed');
}
%WasmTierUpFunction(a.exports.driver);
if (a.exports.driver(0) !== 0) throw new Error('optimized training failed');

const result = a.exports.driver(1);
const tableChanged = table.get(0) === d.exports.target;
assertEquals(getterCount, 0);
assertFalse(tableChanged);
