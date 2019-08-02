// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  const proto = [];
  const obj = Object.create(proto);
  obj[1] = "";
  proto[1];
  proto.bla = 42;
}

foo();
%OptimizeFunctionOnNextCall(foo);
foo();
