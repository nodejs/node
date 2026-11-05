// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const kHeapNumberType = Sandbox.getInstanceTypeIdFor("HEAP_NUMBER_TYPE");
const kHeapNumberValueOffset = Sandbox.getFieldOffset(kHeapNumberType, "value");

// The byteLength must exceed Smi::kMaxValue (1 GB on 64-bit architectures)
// so that Runtime_GrowableSharedArrayBufferByteLength allocates a HeapNumber.
const kByteLength = 0x40001000;
const kTimeoutMs = 1000;

function testCorruptedGsabLength(corruptedFloat) {
  const sharedBuffer = new SharedArrayBuffer(16);
  const sharedState = new Int32Array(sharedBuffer);

  const workerScript = `
    const byteLength = ${kByteLength};
    const buffer = new SharedArrayBuffer(byteLength, {maxByteLength: byteLength});
    const view = new Uint8Array(buffer);

    function getLength(v) { return v.length; }
    %PrepareFunctionForOptimization(getLength);
    getLength(view);
    getLength(view);
    %OptimizeFunctionOnNextCall(getLength);
    getLength(view);

    postMessage("ready");

    onmessage = function(msg) {
      const sharedState = new Int32Array(msg.data);
      const marker = {};
      // In the bump-pointer new space allocation arena, the HeapNumber allocated
      // by Runtime_GrowableSharedArrayBufferByteLength will be placed immediately
      // after the newly allocated marker object.
      const heapNumAddr = Sandbox.getAddressOf(marker) + Sandbox.getSizeOf(marker);
      Atomics.store(sharedState, 0, heapNumAddr);

      // getLength invokes Runtime_GrowableSharedArrayBufferByteLength, which
      // allocates the HeapNumber and pauses at the synchronization point.
      const len = getLength(view);
      postMessage({resultLen: len});
    };
  `;

  const worker = new Worker(workerScript, {type: "string"});
  assertEquals("ready", worker.getMessage());

  %BlockAt("GrowableSharedArrayBufferByteLength", kTimeoutMs);
  worker.postMessage(sharedBuffer);

  assertTrue(%WaitUntilBlocked("GrowableSharedArrayBufferByteLength", kTimeoutMs));

  let heapNumAddr = 0;
  while ((heapNumAddr = Atomics.load(sharedState, 0)) === 0) {}

  const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
  assertEquals(kByteLength, mem.getFloat64(heapNumAddr + kHeapNumberValueOffset, true));

  // Corrupt the HeapNumber's float64 payload to an invalid/negative value.
  mem.setFloat64(heapNumAddr + kHeapNumberValueOffset, corruptedFloat, true);

  assertTrue(%Resume("GrowableSharedArrayBufferByteLength"));

  const res = worker.getMessage();
  // With the fix, any corrupted/negative byte_length exceeding kMaxByteLength
  // causes the bounds check to fail and length safely evaluates to 0.
  assertEquals(0, res.resultLen);
  worker.terminate();
}

testCorruptedGsabLength(-65536.0);
testCorruptedGsabLength(-1.0);
testCorruptedGsabLength(-1e15);
testCorruptedGsabLength(NaN);
