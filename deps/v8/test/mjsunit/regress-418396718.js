// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var count = 0;
function bar() {
  count++;
  if (count > 100000) {
    throw Error();
  }
}
%NeverOptimizeFunction(bar);

function foo() {
    let v1 = 0;
    for (let i4 = 0;
        (() => {
            const v6 = i4 < 25;
            bar();
            return v6;
        })();
        ) {
        const v10 = v1 + 2147483647;
        v1 = i4;
        1024 & v10;
    }
    return 1024;
}
try { foo(); } catch {}
