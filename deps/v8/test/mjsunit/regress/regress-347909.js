// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = {y:1.5};
a.y = 0;
var b = a.y;
a.y = {};
var d = 1;
function f() {
  d = 0;
  return {y: b};
}
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
