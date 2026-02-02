// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

const obj7 = -1;
function foo(arg) {
    let obj10 = 0;
    let obj11 = 0;
    for(var i= 0 ;i<2;i++){
        const obj15 = 1;
        const obj17 = obj11 + 1;
        const obj18 = 2;
        const obj19 = 3;
        const obj20 = obj10 / 3;
        obj10 = obj20;
        let obj21 = 0;
        do {
            try {
                const obj23 = !obj7;

            } catch(e) {
            }
            obj21++;
        } while (obj21 < 2);
    }

}
const obj32 = [1,2,3,4];


%PrepareFunctionForOptimization(foo);
foo(obj32);foo(obj32);foo(obj32);
console.log("maglev");
%OptimizeMaglevOnNextCall(foo);
foo(obj32);
