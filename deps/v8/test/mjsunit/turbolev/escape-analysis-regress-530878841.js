// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

// An Int32 greater than FixedArray::kMaxLength (0x8000000).
const large_non_smi = 0x40000000;

let arr = [];

function foo() {
  return arr[large_non_smi];
}

%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
