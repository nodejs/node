// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function g() {
  var f;
  class C extends eval("f = () => delete C; Array") {}
  f();
}

assertThrows(g, SyntaxError);
%OptimizeFunctionOnNextCall(g);
assertThrows(g, SyntaxError);
