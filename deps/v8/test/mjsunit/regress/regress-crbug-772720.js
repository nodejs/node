// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var global;
function f() {
  var local = 'abcdefghijklmnopqrst';
  local += 'abcdefghijkl' + (0 + global);
  global += 'abcdefghijkl';
}
f();
%OptimizeFunctionOnNextCall(f);
f();
