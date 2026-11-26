// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --hole-fuzzing --maglev
// NO HARNESS

// Ensure Maglev's Abort node initializes kContextRegister before calling
// Runtime::kAbort, even when kContextRegister holds an odd raw Int32 (which
// looks HeapObject-tagged to IsContext) from register allocation. Under
// --hole-fuzzing this must exit cleanly (FAIL) rather than segfaulting in
// Runtime_Abort's context DCHECK (CRASH).

var leaked_hole = %LeakHole();
var g = [0];

function trigger(a) {
  let v0 = a + 1;
  let v1 = a + 3;
  let v2 = a + 5;
  let v3 = a + 7;
  let v4 = a + 9;
  let v5 = a + 11;
  let v6 = a + 13;
  let v7 = a + 15;
  let v8 = a + 17;
  let v9 = a + 19;
  let v10 = a + 21;
  let v11 = a + 23;
  let v12 = a + 25;
  let v13 = a + 27;
  let v14 = a + 29;
  let v15 = a + 31;
  let v16 = a + 33;
  let v17 = a + 35;
  let v18 = a + 37;
  let v19 = a + 39;
  let v20 = a + 41;
  let v21 = a + 43;
  let v22 = a + 45;
  let v23 = a + 47;
  let v24 = a + 49;
  let v25 = a + 51;
  g[0] = v0 | v1 | v2 | v3 | v4 | v5 | v6 | v7 | v8 | v9 | v10 | v11 | v12 |
         v13 | v14 | v15 | v16 | v17 | v18 | v19 | v20 | v21 | v22 | v23 |
         v24 | v25;
  return Boolean(leaked_hole);
}

%PrepareFunctionForOptimization(trigger);
trigger(0);
%OptimizeMaglevOnNextCall(trigger);
trigger(0);
