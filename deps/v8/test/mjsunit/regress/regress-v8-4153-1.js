// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

// Create tiny (on-heap) instances of TypedArrays to make sure
// that the ByteArrays are properly sized (in new space).
var arrays = [
  Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array,
  Float32Array, Float64Array, Uint8ClampedArray, BigInt64Array, BigUint64Array
].map(C => {
  new C(1)
});
