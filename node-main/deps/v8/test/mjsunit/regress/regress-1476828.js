// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(arg) {
  if (arg.f)
    return true;
  else
    return false;
}

let o = {f: %GetUndetectable()};

%PrepareFunctionForOptimization(foo);
assertFalse(foo(o));
assertFalse(foo(o));
%OptimizeMaglevOnNextCall(foo);
assertFalse(foo(o));
