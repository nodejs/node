// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing  --allow-natives-syntax --stress-concurrent-inlining-attach-code

function __wrapTC(f = true) {
    return f();
}
const __v_10 = () => ({
  [__v_9]() {
  },
});
const __v_44 = () =>{};
  async function* __f_74() {
      const __v_375 = {
        toString() {
        }
      };
      try {
        const __v_379 = d8.execute();
        for (let __v_380 = 0; (() => {
   [ __v_379];
        })();) {
          yield __v_373;
        }
      } catch (__v_381) {}
      return __v_44;
  }
    __f_74().next();
  const __v_374 = __wrapTC(() => %OptimizeMaglevOnNextCall(__f_74));
