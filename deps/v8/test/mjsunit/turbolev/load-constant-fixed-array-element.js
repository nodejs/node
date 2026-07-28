// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function get_const() {
  return 3;
}

const arr = [0, 2, 4, 8, 16, 32];

let glob_obj = { x : 42 }
glob_obj.x = 17; // Non-const Smi field.

function foo(deopt) {
  let index = get_const();

  // Storing {index} to a Smi field so that we'll insert an UnsafeSmiUntag when
  // using it as an array index rather than a CheckedSmiUntag or a
  // CheckedObjectToIndex.
  glob_obj.x = index;
  return arr[index];
}

%PrepareFunctionForOptimization(get_const);
%PrepareFunctionForOptimization(foo);
assertEquals(8, foo(false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(8, foo(false));
assertOptimized(foo);
