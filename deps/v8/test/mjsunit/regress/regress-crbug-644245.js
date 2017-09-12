// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f() {
  try {
    throw "boom";
  } catch(e) {
    %_DeoptimizeNow();
  }
}

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
