// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// DataView.prototype.byteLength and DataView.prototype.byteOffset throw on
// detached ArrayBuffers, unlike the TypedArray.prototype counterparts. Turbofan
// should not reduce them the same way.

let ab = new ArrayBuffer();
let dv = new DataView(ab);
%ArrayBufferDetach(ab);

function TestByteLength(dv) {
  let caught = 0;
  for (let i = 0; i < 64; i++) {
    try {
      dv.byteLength;
    } catch (e) {
      caught++;
    }
    if (i == 2) %OptimizeOsr();
  }
  assertEquals(64, caught);
}
%PrepareFunctionForOptimization(TestByteLength);
TestByteLength(dv);

function TestByteOffset(dv) {
  let caught = 0;
  for (let i = 0; i < 64; i++) {
    try {
      dv.byteOffset;
    } catch (e) {
      caught++;
    }
    if (i == 2) %OptimizeOsr();
  }
  assertEquals(64, caught);
}
%PrepareFunctionForOptimization(TestByteOffset);
TestByteOffset(dv);
