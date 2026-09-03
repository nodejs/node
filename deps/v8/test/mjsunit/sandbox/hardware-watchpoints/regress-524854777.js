// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints

// This test verifies that d8 hardware watchpoints correctly handle YMM
// registers, which are used by optimized memory copy routines.
// We trigger a YMM hit by watching the elements of a large double Array and
// then performing an operation like slice() that uses a vectorized copy.

const kArraySize = 2000;
const arr = [];
for (let i = 0; i < kArraySize; i++) arr.push(i + 0.1);

// Use the Sandbox API to find the backing FixedDoubleArray for the JSArray.
const elements = Sandbox.dereferenceTaggedPointerField(arr, 'elements');
assertNotNull(elements, 'Could not get elements FixedDoubleArray');

const kFixedDoubleArrayType =
    Sandbox.getInstanceTypeIdFor('FIXED_DOUBLE_ARRAY_TYPE');
const kDataOffset = Sandbox.getFieldOffset(kFixedDoubleArrayType, 'data');

// Watch multiple elements in the middle of the array to increase the
// chance of hitting a YMM load. YMM moves 32 bytes (4 doubles).
for (let i = 0; i < 4; i++) {
  // Spreading them out ensures we hit different 32-byte chunks.
  Sandbox.markForCorruptionOnAccess(elements, kDataOffset + (1000 + i * 4) * 8);
}

// Trigger a vectorized move. Array.slice() on a large range of doubles
// uses optimized bulk move routines (specifically vmovdqu with YMM
// registers on AVX-capable hardware).
let mutation_detected = false;
for (let i = 0; i < 10 && !mutation_detected; i++) {
  // Slice a range that includes our watched elements.
  const result = arr.slice(800, 1200);

  // The hardware watchpoint logic will mutate the value read from memory
  // before it is stored in the result register (YMM).
  for (let j = 0; j < result.length; j++) {
    if (result[j] !== (800 + j + 0.1)) {
      mutation_detected = true;
      break;
    }
  }
}

assertTrue(
    mutation_detected,
    'The value in the YMM register should have been mutated');
