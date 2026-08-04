// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {}
function g() {
  f.prototype = undefined;
  f();
  new f();
}

// Do not use %OptimizeFunctionOnNextCall here, this particular bug needs
// to trigger truly concurrent compilation.
for (let i = 0; i < 10000; i++) g();
