// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --maglev-poly-calls

let v = { a: 11 };
function foo(o) {
  try {
    return o.foo(v.a);
  } catch (e) {
  }
}
%PrepareFunctionForOptimization(foo);
foo({});
v = {};
foo({});
%OptimizeMaglevOnNextCall(foo);
foo();
