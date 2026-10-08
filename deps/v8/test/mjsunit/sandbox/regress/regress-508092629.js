// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const r32 = addr => mem.getUint32(addr, true);
const w32 = (addr, value) => mem.setUint32(addr, value >>> 0, true);
const ptr = value => value & ~3;

function typeOf(addr) {
  try {
    return Sandbox.getInstanceTypeOfObjectAt(addr);
  } catch (e) {
    return 'invalid';
  }
}

const jsFuncTypeId = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const jsFuncContextOffset = Sandbox.getFieldOffset(jsFuncTypeId, 'context');

function nativeContextOf(fn) {
  const functionAddr = Sandbox.getAddressOf(fn);
  const context = ptr(r32(functionAddr + jsFuncContextOffset));
  if (typeOf(context) === 'NATIVE_CONTEXT_TYPE') return context;
  const candidate = ptr(r32(context + 0x0c));
  if (typeOf(candidate) === 'NATIVE_CONTEXT_TYPE') return candidate;
  for (let offset = 0; offset < 0x40; offset += 4) {
    const value = r32(context + offset);
    if ((value & 1) === 0) continue;
    const maybeNative = ptr(value);
    if (typeOf(maybeNative) === 'NATIVE_CONTEXT_TYPE') return maybeNative;
  }
  throw Error('no native ' + context.toString(16) + ' ' + typeOf(context));
}

const nativeContextTypeId = Sandbox.getInstanceTypeIdFor('NATIVE_CONTEXT_TYPE');
const microtaskQueueOffset =
    Sandbox.getFieldOffset(nativeContextTypeId, 'microtask_queue');
const slotOf = native => native + microtaskQueueOffset;

// 1. Setup two realms with default and custom MQ.
function main_marker() {}
const mainNative = nativeContextOf(main_marker);
const mainSlot = slotOf(mainNative);
const mainHandle = r32(mainSlot);

const r_custom = Realm.create(undefined, {create_own_microtask_queue: true});

Realm.shared = {
  nativeContextOf: nativeContextOf,
  slotOf: slotOf,
  r32: r32
};

const res = Realm.eval(r_custom, `
  const helpers = Realm.shared;
  function custom_marker() {}
  const native = helpers.nativeContextOf(custom_marker);
  const slot = helpers.slotOf(native);
  [slot, helpers.r32(slot)];
`);
const customSlot = res[0];
const customHandle = res[1];

print(
    'main.slot=0x' + mainSlot.toString(16) + ' custom.handle=0x' +
    customHandle.toString(16));

// 2. Swap custom MQ handle and default MQ handle in NativeContexts before
// dispose.
w32(mainSlot, customHandle);
w32(customSlot, mainHandle);

// 3. Let the custom realm die
Realm.dispose(r_custom);
for (let i = 0; i < 16; i++) {
  gc();
}

// 4. Post multiple microtasks in main context to fill capacity and trigger
// C++ reallocation crash.
for (let i = 0; i < 20; i++) {
  Promise.resolve().then(() => print('microtask_ran ' + i));
}

print('DONE');
