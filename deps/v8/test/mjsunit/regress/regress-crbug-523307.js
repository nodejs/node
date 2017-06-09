// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  var c = x * x << 366;
  var a = c + c;
  return a;
}

f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
f(1);
