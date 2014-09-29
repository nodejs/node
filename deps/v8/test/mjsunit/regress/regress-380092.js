// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function many_hoist(o, index) {
  return o[index + 33554427];
}

var obj = {};
many_hoist(obj, 0);
%OptimizeFunctionOnNextCall(many_hoist);
many_hoist(obj, 5);

function constant_too_large(o, index) {
  return o[index + 1033554433];
}

constant_too_large(obj, 0);
%OptimizeFunctionOnNextCall(constant_too_large);
constant_too_large(obj, 5);
