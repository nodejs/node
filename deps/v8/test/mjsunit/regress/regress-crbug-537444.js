// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-tailcalls --allow-natives-syntax

"use strict";

function f(x) {
  return x;
}

function g(x) {
  return false ? 0 : f(x, 1);
}

function h(x) {
  var z = g(x, 1);
  return z + 1;
}

%SetForceInlineFlag(g);
%SetForceInlineFlag(f);

h(1);
h(1);
%OptimizeFunctionOnNextCall(h);
h("a");
