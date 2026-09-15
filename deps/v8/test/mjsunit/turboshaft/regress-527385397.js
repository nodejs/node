// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// {glob} will be used to publish the allocation to the GC. Its {x} and {y}
// field must not be at the same offset than `arr[2]` below. And its {y} field
// must be a Tagged field (rather than Float64) so that it forces tagging when
// we store to it, which can trigger a GC. (the "offset_16" field is at offset
// 16 and is needed to that the "y" field isn't).
let glob = { x : 42, offset_16 : "skipping offset 16", y : "abc" };

function foo(x, ...rest) {
  // Turboshaft can recognize rotations in xor trees, which Turbofan cannot. As
  // a result, {lhs} and {rhs} below will not be recognized as being equal by
  // Turbofan, but Turboshaft will realize that it's the case...
  let lhs = (x << 2) ^ x ^ (x >>> 30);
  let rhs = (x >>> 30) ^ x ^ (x << 2);
  // ... which means that {cond} will be constant-folded to true but only in
  // Turboshaft...
  let cond = lhs == rhs;
  // ... and len will thus be a Phi for Turbofan but 3 for Turboshaft.
  let len = cond ? 3 : 4;

  // Note that length 3 is the only one that works because:
  //
  //   - Only loops with length 3 or less are fully unrolled
  //
  //   - `new Array` does 2 allocation: the FixedArray backing store and the
  //     JSArray, and to initialize the latter it does 4 stores, at offsets 0
  //     (Map), 4 (properties_or_hash), 8 (elements == the FixedArra backing
  //     store) and 12 (length), and it's important that none of those have the
  //     same offset as the final `arr[2] = 42` (which will be at offset 16).

  // Allocating fixed-sized array. This will be lowered to a NewArray, which
  // itself will be lowered to a loop with 3 iterations, which will then be
  // unrolled.
  let arr = new Array(len);

  // Publishing the array for the GC...
  glob.x = arr;
  // ... so that it's seen by the GC here.
  %MajorGCForCompilerTesting();

  // Storing at offset 2 in {arr}. This should not store-store eliminate with
  // the initializing store since there is a GC in the middle.
  arr[2] = 42;

  // Making sure that {arr} escapes.
  return arr;
}

// We need to warmup feedback with a non-smi int32 in order to avoid that `x<<2`
// is lowered to `(x>>1)<<2` because of the untagging, since it would then be
// optimized to `x<<1`, and the rotation would thus not be matched anymore since
// `x<<1^x>>30` cannot be optimized to a rotation. With HeapNumber feedback, the
// untagging will handle both Smi and HeapNumber and thus return a Phi, so
// Turboshaft won't be able to optimize `x>>1<<2` to `x<<1` anymore.
let non_smi_int32 = 0x80000000;

%PrepareFunctionForOptimization(foo);
foo(non_smi_int32);
foo(42);

%OptimizeFunctionOnNextCall(foo);
foo(42);
