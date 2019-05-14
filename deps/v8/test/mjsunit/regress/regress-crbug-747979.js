// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a) {
  %HeapObjectVerify(a);
  a[1] = 0;
  %HeapObjectVerify(a);
}

function foo() {}

var arr1 = [0];
var arr2 = [0];
var arr3 = [0];

arr1.f = foo;
arr1[0] = 4.2;

arr2.f = foo;

arr3.f = foo;
arr3[0] = 4.2;
arr3.f = f;

f(arr1);
f(arr2);
f(arr3);
%OptimizeFunctionOnNextCall(f);
f(arr3);
