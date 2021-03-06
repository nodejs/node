// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=200 --stack-size=200 --budget-for-feedback-vector-allocation=100 --expose-gc --stress-flush-bytecode

var i = 0;
function main() {
function v0() {
   function v10(a) {
        i++;
        var cur_i = i;
        try {
            // This triggers the use of old closure that was installed in the
            // earlier invocation of v10 and causes an infinite recursion. At
            // some point we throw from here.
            [].e = 1;

            // Throw when the new closure is on the stack to trigger a OSR on
            // the new closure
            if (cur_i == 2) throw 1;
        } catch(v22) {
            // This loop triggers OSR.
            for (const v24 in "c19rXGEC2E") {
            }
        }
   }
   const v25 = v10(1);
   // We install v10's closure here. The bytecode for v10 gets flushed on gc()
   const v21 = Object.defineProperty([].__proto__,"e",{set:v10});
}
const v26 = v0();
// With --stress-flush-bytecode GC flushes the bytecode for v0 and v10
gc();
assertThrows(v0, TypeError);
}
main();
