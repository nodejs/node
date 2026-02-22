// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let ab = new ArrayBuffer(7);
let dv = new DataView(ab);

/* -----------------------------
   | 0 | 1 | 2 | 3 | 4 | 5 | 6 | << the ArrayBuffer
   -----------------------------

                           ---------
                           | X | X | << trying to access size 2 element at index 6
                           ---------
*/

function foo(dv) {
  return dv.getInt16(6);
}
%PrepareFunctionForOptimization(foo);

assertThrows(() => foo(dv), RangeError);

%OptimizeMaglevOnNextCall(foo);

assertThrows(() => foo(dv), RangeError);
