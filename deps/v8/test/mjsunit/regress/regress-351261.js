// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fold-constants

function store(a) {
  a[5000000] = 1;
}

function foo() {
  var __v_8 = new Object;
  var __v_7 = new Array(4999990);
  store(__v_8);
  store(__v_7);
}
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
