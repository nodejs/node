// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

globalThis.eSmi = 0;

function singleInputPhiSmi(c) {
  let x = 0;
  for (let i = 0; i < 1; i++) {
    for (let j = 0; j < 0; j++) {}
    if (c) break;
    if (c == 1) x = 1;
  }
  globalThis.eSmi = x;
  return x != null;
}

%PrepareFunctionForOptimization(singleInputPhiSmi);
assertTrue(singleInputPhiSmi(true));
assertTrue(singleInputPhiSmi(true));
%OptimizeMaglevOnNextCall(singleInputPhiSmi);
assertTrue(singleInputPhiSmi(true));

globalThis.eFloat = -0;

function singleInputPhiHeapNumber(c) {
  let x = -0;
  for (let i = 0; i < 1; i++) {
    for (let j = 0; j < 0; j++) {}
    if (c) break;
    if (c == 1) x = 1.5;
  }
  globalThis.eFloat = x;
  return x != null;
}

%PrepareFunctionForOptimization(singleInputPhiHeapNumber);
assertTrue(singleInputPhiHeapNumber(true));
assertTrue(singleInputPhiHeapNumber(true));
%OptimizeMaglevOnNextCall(singleInputPhiHeapNumber);
assertTrue(singleInputPhiHeapNumber(true));

globalThis.eBool = true;

function singleInputPhiRootConstant(c) {
  let x = true;
  for (let i = 0; i < 1; i++) {
    for (let j = 0; j < 0; j++) {}
    if (c) break;
    if (c == 1) x = false;
  }
  globalThis.eBool = x;
  return !x;
}

%PrepareFunctionForOptimization(singleInputPhiRootConstant);
assertFalse(singleInputPhiRootConstant(true));
assertFalse(singleInputPhiRootConstant(true));
%OptimizeMaglevOnNextCall(singleInputPhiRootConstant);
assertFalse(singleInputPhiRootConstant(true));

globalThis.eStr = "hello";

function singleInputPhiHeapConstant(c) {
  let x = "hello";
  for (let i = 0; i < 1; i++) {
    for (let j = 0; j < 0; j++) {}
    if (c) break;
    if (c == 1) x = "world";
  }
  globalThis.eStr = x;
  return x != null;
}

%PrepareFunctionForOptimization(singleInputPhiHeapConstant);
assertTrue(singleInputPhiHeapConstant(true));
assertTrue(singleInputPhiHeapConstant(true));
%OptimizeMaglevOnNextCall(singleInputPhiHeapConstant);
assertTrue(singleInputPhiHeapConstant(true));

const ta = new Uint8Array(new ArrayBuffer(128));
globalThis.eIntPtr = 128;

function repeatedIntPtrConstStore() {
  const len = ta.length;
  globalThis.eIntPtr = len;
  globalThis.eIntPtr = len;
  return len != null;
}

%PrepareFunctionForOptimization(repeatedIntPtrConstStore);
assertTrue(repeatedIntPtrConstStore());
assertTrue(repeatedIntPtrConstStore());
%OptimizeMaglevOnNextCall(repeatedIntPtrConstStore);
assertTrue(repeatedIntPtrConstStore());
