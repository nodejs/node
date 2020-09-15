// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop

function bar() {}

function foo() {
  try {
    bar( "abc".charAt(4));
  } catch (e) {}
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
