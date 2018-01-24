// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt(a, b) {
  b[0] = 0;

  a.length;

  // TransitionElementsKind
  for (let i = 0; i < 1; i++)
      a[0] = 0;

  b[0] = 9.431092e-317;
}

let arr1 = new Array(1);
arr1[0] = 'a';
opt(arr1, [0]);

let arr2 = [0.1];
opt(arr2, arr2);

%OptimizeFunctionOnNextCall(opt);

opt(arr2, arr2);
assertEquals(9.431092e-317, arr2[0]);
