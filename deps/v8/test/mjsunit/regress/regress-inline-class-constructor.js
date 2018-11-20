// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

var B = class extends Int32Array { }

function f(b) {
  if (b) {
    null instanceof B;
  }
}

f();
f();
f();
%OptimizeFunctionOnNextCall(f);
f();

function f2() {
  return new B();
}

%OptimizeFunctionOnNextCall(f2);
f2();
