// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jit-fuzzing --no-concurrent-osr

class WasmCont {
}
function __wrapTC(f = true) {
    return f();
}
let __v_1 = "?";
let __c = 0;
const __v_2 = __wrapTC(() => async __v_5 => {
  for (let __v_6 = 0; __v_6 < 5; __v_6++) {
    for (let __v_7 = __v_8 = 10; "NFKC";) { if (++__c > 10000) throw 0; }
    function __f_0() {
      return __f_0;
    }
  }
  do {
    await using __v_11 = {};
    __v_1++;
  } while (8);
});
  __v_2().catch(() => {});
