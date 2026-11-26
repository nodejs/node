// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const kSharedFunctionInfoOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE'), 'shared_function_info');

function nonNativeCarrier() {}
const nonNativeSfi = memory.getUint32(
    Sandbox.getAddressOf(nonNativeCarrier) + kSharedFunctionInfoOffset, true);
const argumentsGetter =
    Object.getOwnPropertyDescriptor(Function.prototype, 'arguments').get;

let corrupt = false;

function mapCallback(v) {
  if (corrupt) {
    // Point Array.prototype.map at a non-native SharedFunctionInfo while its
    // inlined builtin continuation frame is active on the stack, then read
    // Array.prototype.map.arguments.
    const mapAddr = Sandbox.getAddressOf(Array.prototype.map);
    memory.setUint32(mapAddr + kSharedFunctionInfoOffset, nonNativeSfi, true);
    argumentsGetter.call(Array.prototype.map);
  }
  return v;
}

function testMap(arr) {
  // Bind 12 arguments so the captured FixedArray's length Smi lands at
  // physical index `height - 1` of the inlined continuation's TranslatedFrame
  // values vector.
  const bound = mapCallback.bind(null, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
  return arr.map(bound);
}

function forEachCallback(v) {
  if (corrupt) {
    // Also exercise Array.prototype.forEach, which is inlined in standalone
    // Maglev as well as TurboFan.
    const forEachAddr = Sandbox.getAddressOf(Array.prototype.forEach);
    memory.setUint32(
        forEachAddr + kSharedFunctionInfoOffset, nonNativeSfi, true);
    argumentsGetter.call(Array.prototype.forEach);
  }
  return v;
}

function testForEach(arr) {
  const bound =
      forEachCallback.bind(null, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
  arr.forEach(bound);
}

const input = [1];
%PrepareFunctionForOptimization(testMap);
testMap(input);
testMap(input);
%OptimizeFunctionOnNextCall(testMap);

%PrepareFunctionForOptimization(testForEach);
testForEach(input);
testForEach(input);
%OptimizeFunctionOnNextCall(testForEach);

corrupt = true;
testMap(input);
testForEach(input);
