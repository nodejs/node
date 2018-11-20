// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(scale) {
  var arr = {a: 1.1};

  for (var i = 0; i < 2; i++) {
    arr[2 * scale] = 0;
  }
}

f({});
f({});
%OptimizeFunctionOnNextCall(f);
f(1004);
