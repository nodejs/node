// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = address => memory.getUint32(address >>> 0, true);
const write32 = (address, value) =>
    memory.setUint32(address >>> 0, value >>> 0, true);

function mapAddress(object) {
  return (read32(Sandbox.getAddressOf(object) >>> 0) - 1) >>> 0;
}

function descriptorArrayAddress(object) {
  return (read32(mapAddress(object) + 0x18) - 1) >>> 0;
}

// V8 DescriptorArray layout (compressed pointers, 4-byte slots). These fragile
// constants live here only, so every access below goes through these helpers.
const kDescriptorCountOffset = 6;    // uint16 number_of_descriptors
const kDescriptorHeaderSize = 0x10;  // offset of the first entry
const kDescriptorEntrySize = 0xc;    // {key, details, value} per entry
const kDescriptorDetailsOffset = 4;
const kDescriptorValueOffset = 8;

function descriptorCount(descriptors) {
  return memory.getUint16(descriptors + kDescriptorCountOffset, true);
}

function descriptorEntryAt(descriptors, index) {
  return descriptors + kDescriptorHeaderSize + index * kDescriptorEntrySize;
}

const descriptorKey = entry => read32(entry);
const descriptorDetails = entry => read32(entry + kDescriptorDetailsOffset);
const descriptorValue = entry => read32(entry + kDescriptorValueOffset);

function findDescriptorEntry(descriptors, keyTagged) {
  const count = descriptorCount(descriptors);
  for (let i = 0; i < count; ++i) {
    const entry = descriptorEntryAt(descriptors, i);
    if (descriptorKey(entry) === keyTagged) return entry;
  }
  throw new Error(`descriptor key 0x${keyTagged.toString(16)} not found`);
}

const originalPayload = 0x3132333435363738n;
const replacementSeed = 0x5152535455565758n;
const overwriteValue = 0x4141414242424243n;

// Give the deep frame a large slot area so the shallow publisher's stale
// pointer aliases live deep-frame slots on an unhardened build.
const kDeepSimdLocals = 256;

const builder = new WasmModuleBuilder();
const noopSig = builder.addType(makeSig([], []));
const nestedSig = builder.addType(makeSig([kWasmI64], []));
const tagSig = builder.addType(makeSig([kWasmI64], []));
const noopIndex = builder.addImport('m', 'noop', noopSig);
const tagIndex = builder.addTag(tagSig);
builder.addExportOfKind('tag', kExternalTag, tagIndex);

// Reentry target invoked from the corrupted values-getter.
builder.addFunction('nested', nestedSig).addBody([]).exportFunc();

// Deep frame: large slot area, throws an i64 payload. Its s2s_Throw handler
// runs the accessor-honoring values lookup while this frame is still live.
const deep = builder.addFunction('deep', makeSig([], []))
                 .addLocals(kWasmS128, kDeepSimdLocals)
                 .addBody([
                   ...wasmI64Const(originalPayload),
                   kExprThrow, ...wasmUnsignedLeb(tagIndex),
                   kExprUnreachable,
                 ]);

// Shallow frame: publishes the stack frame pointer via a JS import that
// returns, then calls deep() with an s2s call so it is not republished.
builder.addFunction('shallow', makeSig([], []))
    .addBody([
      kExprCallFunction, ...wasmUnsignedLeb(noopIndex),
      kExprTry, kWasmVoid,
        kExprCallFunction, ...wasmUnsignedLeb(deep.index),
      kExprCatch, ...wasmUnsignedLeb(tagIndex),
        kExprDrop,
      kExprEnd,
    ])
    .exportFunc();

const instance = builder.instantiate({m: {noop() {}}});
const tagObject = instance.exports.tag;
const sacrificial = new WebAssembly.Exception(tagObject, [replacementSeed]);
const originalDescriptors = descriptorArrayAddress(sacrificial);
const key0Tagged = descriptorKey(descriptorEntryAt(originalDescriptors, 0));
const key1Tagged = descriptorKey(descriptorEntryAt(originalDescriptors, 1));
const key0 = Sandbox.getObjectAt((key0Tagged - 1) >>> 0);
const key1 = Sandbox.getObjectAt((key1Tagged - 1) >>> 0);

let tagSymbol;
let valuesSymbol;
for (const [key, tagged] of [[key0, key0Tagged], [key1, key1Tagged]]) {
  const value = sacrificial[key];
  if (Sandbox.getInstanceTypeOf(value) === 'WASM_EXCEPTION_TAG_TYPE') {
    tagSymbol = key;
  } else if (Sandbox.getInstanceTypeOf(value) === 'FIXED_ARRAY_TYPE') {
    valuesSymbol = key;
  } else {
    throw new Error(
        `unexpected private field for key 0x${tagged.toString(16)}`);
  }
}

const expectedTag = sacrificial[tagSymbol];
const replacementValues = sacrificial[valuesSymbol];
let tagGets = 0;
let valueGets = 0;

function tagGetter() {
  ++tagGets;
  return expectedTag;
}

// Reentry trigger: while deep()'s s2s_Throw builds the exception, this getter
// reenters the instance. The stale shallow pointer still points inside the live
// deep frame on an unhardened build.
function valuesGetter() {
  ++valueGets;
  if (valueGets === 1) {
    instance.exports.nested(overwriteValue);
  }
  return replacementValues;
}

function makeFastAccessorDescriptor(getter) {
  function Carrier() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
    this.d = 4;
  }
  for (let i = 0; i < 20; ++i) new Carrier();
  const carrier = new Carrier();
  Object.defineProperty(
      carrier, 'accessor', {get: getter, configurable: true});
  const descriptors = descriptorArrayAddress(carrier);
  const count = descriptorCount(descriptors);
  for (let i = 0; i < count; ++i) {
    const entry = descriptorEntryAt(descriptors, i);
    const value = descriptorValue(entry);
    if ((value & 1) !== 0 &&
        Sandbox.isValidObjectAt((value - 1) >>> 0) &&
        Sandbox.getInstanceTypeOfObjectAt((value - 1) >>> 0) ===
            'ACCESSOR_PAIR_TYPE') {
      return {details: descriptorDetails(entry), value};
    }
  }
  throw new Error('failed to create a fast AccessorPair descriptor');
}

const tagAccessor = makeFastAccessorDescriptor(tagGetter);
const valuesAccessor = makeFastAccessorDescriptor(valuesGetter);
for (const [keyTagged, descriptor] of [
       [key0Tagged, tagSymbol === key0 ? tagAccessor : valuesAccessor],
       [key1Tagged, tagSymbol === key1 ? tagAccessor : valuesAccessor],
     ]) {
  const dst = findDescriptorEntry(originalDescriptors, keyTagged);
  write32(dst + kDescriptorDetailsOffset, descriptor.details);
  write32(dst + kDescriptorValueOffset, descriptor.value);
}

instance.exports.shallow();

// Both fields must be read straight from their in-object slots. If either
// lookup starts honoring the rewritten descriptors again, the corresponding
// counter becomes non-zero.
assertEquals(0, tagGets, 'tag getter must not run');
assertEquals(0, valueGets, 'values getter must not run');
