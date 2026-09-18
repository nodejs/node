// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/503180648. Optimized Array.prototype.sort
// must not write through a stale pre-left-trim FixedArray pointer when a
// comparator grows the receiver past kMaxCopyElements and then shifts it.

let victim;
let did_mutate;

function makeLargeCapacityPackedSmiArray() {
  const a = [];
  for (let i = 0; i < 236; i++) a.push(1000 + i);
  // Leaves PACKED_SMI_ELEMENTS with length 16 but large backing capacity.
  // (Direct length assignment would right-trim instead.)
  a.splice(16);
  return a;
}

function sortVictim(a) {
  did_mutate = false;
  victim = a;
  // Inline closure so the sort reducer sees a statically-known comparefn via
  // JSCreateClosure/CheckClosure.
  victim.sort(function(a, b) {
    if (!did_mutate) {
      did_mutate = true;
      // No reallocation: backing capacity already covers this.
      for (let i = 0; i < 220; i++) victim.push(2000 + i);
      // Forces LeftTrimFixedArray because new_length > kMaxCopyElements.
      victim.shift();
      // Restore map/length so the inline-sort exit guards pass.
      victim.length = 16;
    }
    return a - b;
  });
  return victim.length;
}

%PrepareFunctionForOptimization(sortVictim);
sortVictim(makeLargeCapacityPackedSmiArray());
sortVictim(makeLargeCapacityPackedSmiArray());
%OptimizeFunctionOnNextCall(sortVictim);
sortVictim(makeLargeCapacityPackedSmiArray());
%CollectGarbage(0);
