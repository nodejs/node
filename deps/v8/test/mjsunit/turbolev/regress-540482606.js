// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --assert-types

function foo() {
  for (let v19 = 0; v19 < 4; v19++) {
    for (let v22 = 0; v22 < 5; v22++) {
      const v24 = v22++;
      // v26 is a phi typed as Boolean | Number. v24 > v22 is always false,
      // so at run time it's always a Number, and the first arm of this
      // branch is dead.
      const v26 = v24 > v22 || v24;
      // The ^ operation has number feedback since we always saw a number.
      const v27 = v26 ^ 3221225472;
      [v26, 1.1];
      v22 = 3221225472;
      %OptimizeOsr();
    }
  }
}
%PrepareFunctionForOptimization(foo);
foo();
