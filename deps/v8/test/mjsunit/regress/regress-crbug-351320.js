// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fold-constants

var result = 0;
var o1 = {};
o2 = {y:1.5};
o2.y = 0;
o3 = o2.y;

function crash() {
  for (var i = 0; i < 10; i++) {
    result += o1.x + o3.foo;
  }
}

crash();
%OptimizeFunctionOnNextCall(crash);
crash();
