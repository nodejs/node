// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
'use strict'

function outer_func() {
    function inner_func() {
        let step = 1;

        for (let i = 0; i < 10; i = i + step) {
            const v14 = step++;
            const v15 = v14 * i;
            function dumb_func() {
            }
        }
    }
    for (let v24 = 0; v24 < 10; v24++) {
        inner_func();
    }
}
for (let j = 0; j < 10000; j++) {
    outer_func();
}
