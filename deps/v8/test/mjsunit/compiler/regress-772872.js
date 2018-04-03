// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  for (var x = 10; x > 5; x -= 16) {}
}
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
