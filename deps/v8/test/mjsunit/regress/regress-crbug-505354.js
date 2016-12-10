// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-filter=f

function f() {
  "use strict";
  try {
    for (let i = 0; i < 10; i++) {}
  } catch(e) {}
}
%OptimizeFunctionOnNextCall(f);
f();
