// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  var s = Math.floor(x / 3600);
  Math.floor(s);
  return s % 24;
}

foo(12345678);
foo(12345678);
%OptimizeFunctionOnNextCall(foo);
foo(12345678);
