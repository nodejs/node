// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var n = 3;
  var obj = {};

  var m = n;
  for (;;) {
    m++;

    if (m == 456) {
      break;
    }

    var i = 0;
    var j = 0;
    while (i < 1) {
      j = i;
      i++;
    }
    obj.y = j;
  }
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
