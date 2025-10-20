// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let v1 = {a: 1, b: 2};
const v2 = 29234234234234;

function foo(val) {
  let v3 = val + 5;
  let v4 = dummy;
  let v5 = dummy;
  try {
    val++;
    v4 = v5;
    v5.prop = {};
    v5.prop = {};
    v5.prop = {};
    v5.prop = {};
    v5.prop = {};
  } catch(e) { val = v3; }
  return {a: val, b: val, c: v1};
}

%PrepareFunctionForOptimization(foo);
foo(v2);
%OptimizeFunctionOnNextCall(foo);
foo(v1.a);
