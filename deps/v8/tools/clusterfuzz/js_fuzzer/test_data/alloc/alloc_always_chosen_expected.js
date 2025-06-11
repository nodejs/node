// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* AllocationTimeoutMutator: Random timeout */
__callSetAllocationTimeout(1943, false);
// Original: alloc/alloc.js
console.log(42);
try {
  /* AllocationTimeoutMutator: Random timeout */
  __callSetAllocationTimeout(2682, false);
  console.log(42);
} catch (__v_0) {}
function __f_0() {
  return 0;
}
for (let __v_1 = 0; __v_1 < 10000; __v_1++) {
  console.log(42);
}
/* AllocationTimeoutMutator: Random timeout */
__callSetAllocationTimeout(781, true);
%PrepareFunctionForOptimization(__f_0);
/* AllocationTimeoutMutator: Random timeout */
__callSetAllocationTimeout(2401, false);
__f_0();
/* AllocationTimeoutMutator: Random timeout */
__callSetAllocationTimeout(2721, false);
__f_0();
%OptimizeFunctionOnNextCall(__f_0);
/* AllocationTimeoutMutator: Random timeout */
__callSetAllocationTimeout(841, false);
/* AllocationTimeoutMutator: Random timeout */
__callSetAllocationTimeout(229, true);
__f_0();
