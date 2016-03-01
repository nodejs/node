// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --strong-mode

// This tests that inlining a constructor call to a function which cannot be
// used as a constructor (e.g. strong mode function) still throws correctly.

function g() {
  "use strong";
}

function f() {
  return new g();
}

assertThrows(f);
assertThrows(f);
%OptimizeFunctionOnNextCall(f);
assertThrows(f);
