// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-filter=* --allow-natives-syntax

var v1 = {};
function g() {
  v1 = [];
  for (var i = 0; i < 1; i++) {
    v1[i]();
  }
}

var v2 = {};
var v3 = {};
function f() {
  v3 = v2;
  g();
}

assertThrows(g);
%OptimizeFunctionOnNextCall(f);
assertThrows(f);
