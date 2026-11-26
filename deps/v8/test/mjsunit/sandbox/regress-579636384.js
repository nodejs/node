// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax
// Flags: --no-maglev --maglev-inline-api-calls

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const functionType = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
const dispatchHandleOffset =
    Sandbox.getFieldOffset(functionType, 'dispatch_handle');

function handleOf(fn) {
  return memory.getUint32(
      Sandbox.getAddressOf(fn) + dispatchHandleOffset, true);
}
function setHandle(fn, handle) {
  memory.setUint32(
      Sandbox.getAddressOf(fn) + dispatchHandleOffset, handle, true);
}

// command, address, handle A, epoch, ack, stores, handle B
const shared = new SharedArrayBuffer(7 * 4);
const control = new Int32Array(shared);
const writer = new Worker(`
  onmessage = event => {
    const control = new Int32Array(event.data);
    const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    let toggle = 0;
    postMessage('ready');
    for (;;) {
      const command = Atomics.load(control, 0);
      if (command === 2) break;
      if (command === 0) {
        Atomics.store(control, 4, Atomics.load(control, 3));
        Atomics.notify(control, 4);
        Atomics.wait(control, 0, 0, 10);
        continue;
      }
      const address = Atomics.load(control, 1) >>> 0;
      const value = Atomics.load(control, toggle ? 2 : 6) >>> 0;
      toggle ^= 1;
      memory.setUint32(address, value, true);
      const count = Atomics.add(control, 5, 1) + 1;
      if ((count & 0xff) === 0) Atomics.notify(control, 5);
    }
  };
`, {type: 'string'});
writer.postMessage(shared);
assertEquals('ready', writer.getMessage());

function startAlternating(address, handleA, handleB) {
  const initialCount = Atomics.load(control, 5);
  Atomics.store(control, 1, address | 0);
  Atomics.store(control, 2, handleA | 0);
  Atomics.store(control, 6, handleB | 0);
  Atomics.store(control, 0, 1);
  Atomics.notify(control, 0);
  while (Atomics.load(control, 5) - initialCount < 2048) {
    const oldCount = Atomics.load(control, 5);
    Atomics.wait(control, 5, oldCount, 10);
  }
}

function stopAlternating() {
  const epoch = Atomics.add(control, 3, 1) + 1;
  Atomics.store(control, 0, 0);
  Atomics.notify(control, 0);
  while (Atomics.load(control, 4) !== epoch) {
    const oldAck = Atomics.load(control, 4);
    Atomics.wait(control, 4, oldAck, 100);
  }
}

for (let attempt = 0; attempt < 150; ++attempt) {
  const victimMarker = 0x100000 + attempt;
  const donorMarker = 0x200000 + attempt;
  const victim = eval(`(function victim${attempt}(x) {
    if (x === -1) %DeoptimizeNow();
    return (x | 0) + ${victimMarker};
  })`);
  const donor = new Function('x', `return (x | 0) + ${donorMarker};`);

  %PrepareFunctionForOptimization(victim);
  %PrepareFunctionForOptimization(donor);
  victim(1);
  victim(2);
  donor(1);
  donor(2);

  const victimHandle = handleOf(victim);
  const donorHandle = handleOf(donor);
  %DisableOptimizationFinalization();
  %OptimizeFunctionOnNextCall(victim, 'concurrent');
  victim(3);
  %WaitForBackgroundOptimization();

  const slot = Sandbox.getAddressOf(victim) + dispatchHandleOffset;
  startAlternating(slot, victimHandle, donorHandle);
  %FinalizeOptimization();
  stopAlternating();
  setHandle(victim, victimHandle);

  if (donor(7) === victimMarker + 7) {
    donor(-1);  // Marks the victim Code for deoptimization.
    assertEquals(donorMarker + 7, donor(7));
  }
}

Atomics.store(control, 0, 2);
Atomics.notify(control, 0);
writer.terminateAndWait();
