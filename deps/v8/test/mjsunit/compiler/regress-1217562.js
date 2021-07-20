// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  foo.bind();
  foo.__proto__ = class {};
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
foo();
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
