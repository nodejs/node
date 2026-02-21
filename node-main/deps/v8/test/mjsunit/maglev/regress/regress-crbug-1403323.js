// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function foo(a) {
  if (a.length > 0) {}
}
%PrepareFunctionForOptimization(foo);
foo(false);
foo(false);
foo(4);
%OptimizeMaglevOnNextCall(foo);
assertThrows(foo);
