// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --sparkplug-plus

function load(obj) {
  return obj.x;
}

%PrepareFunctionForOptimization(load);

const o0 = {x: 42};
const o1 = {y: 1, x: 43}; // Different map

// Step 1: Run in interpreter to populate feedback to MONOMORPHIC (o0).
// Baseline code is not compiled yet.
load(o0);

let feedback = %GetFeedback(load);
if (feedback !== undefined) {
  assertMatches(/^MONOMORPHIC/, feedback[0][1]);
}

// Step 2: Compile baseline. The baseline call sites will be UNINITIALIZED.
%CompileBaseline(load);

// Step 3: Second call executes Sparkplug+ code (baseline) with o0.
// It executes the uninitialized LoadIC on monomorphic feedback.
// It should miss, and we should heal it.
// If we heal it correctly to MONOMORPHIC handler:
//   - Baseline is patched to monomorphic o0 handler.
//   - Feedback stays MONOMORPHIC.
// If we buggily patch it to GENERIC handler (due to early return in UpdatePolymorphicIC):
//   - Baseline is patched to generic handler.
//   - Feedback stays MONOMORPHIC.
load(o0);

feedback = %GetFeedback(load);
if (feedback !== undefined) {
  print("Feedback after 2nd call (o0): " + JSON.stringify(feedback));
  assertMatches(/^MONOMORPHIC/, feedback[0][1]);
}

// Step 4: Third call with o1 (different map).
// If baseline was correctly healed to monomorphic o0 handler:
//   - It should miss in baseline (since o1 map != o0 map).
//   - Go to runtime, see new map o1, transition feedback to POLYMORPHIC (or MEGAMORPHIC).
// If baseline was buggily patched to generic handler:
//   - It will HIT in baseline generic handler.
//   - Will NOT go to runtime, feedback will STAYS MONOMORPHIC (bug!).
load(o1);

feedback = %GetFeedback(load);
if (feedback !== undefined) {
  print("Feedback after 3rd call (o1): " + JSON.stringify(feedback));
  // We expect it to NOT be MONOMORPHIC anymore.
  // It should be POLYMORPHIC (or MEGAMORPHIC).
  let state = feedback[0][1];
  assertFalse(state.startsWith("MONOMORPHIC"), "Feedback should have transitioned, but stayed MONOMORPHIC!");
}
