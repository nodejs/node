// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const byteLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype), 'byteLength').get;

function getByteLength(ta) {
  ta.byteOffset;
  return byteLength.call(ta);
}

const abToDetach = new ArrayBuffer(10);
const ta1 = new Uint8Array(abToDetach);
const ta2 = new Uint8Array(abToDetach);
%ArrayBufferDetach(abToDetach);

const rab = new ArrayBuffer(10, {maxByteLength: 20});
const rabTa = new Uint8Array(rab);

const normalAb = new ArrayBuffer(10);
const normalTa1 = new Uint8Array(normalAb);
const normalTa2 = new Uint8Array(normalAb);

%PrepareFunctionForOptimization(getByteLength);
getByteLength(rabTa);
getByteLength(normalTa1);
%OptimizeFunctionOnNextCall(getByteLength);
assertEquals(10, getByteLength(normalTa1));

%ArrayBufferDetach(normalAb);
assertEquals(0, getByteLength(normalTa1));
